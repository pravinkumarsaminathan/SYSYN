#!/usr/bin/env python3
"""
NLP Shell — MCP Server (HTTP/Streamable HTTP Transport)
Uses Groq's FREE API (llama3-70b) to translate NLP → Linux commands.
Testable directly from Postman.

Transport: Streamable HTTP (POST /mcp)
Protocol:  JSON-RPC 2.0
"""

import json
import os
import subprocess
import uuid
from http.server import BaseHTTPRequestHandler, HTTPServer

import requests  # pip install requests

# ─── Config ───────────────────────────────────────────────────────────────────
GROQ_API_KEY = os.environ.get("GROQ_API_KEY", "")   # export GROQ_API_KEY=gsk_...
GROQ_MODEL   = "openai/gpt-oss-120b"                      # free on Groq
GROQ_URL     = "https://api.groq.com/openai/v1/chat/completions"
PORT         = 8080

SYSTEM_PROMPT = """You are a Linux command expert embedded in a shell assistant.

Given a natural language query, respond ONLY with a valid JSON object (no markdown, no extra text):

{
  "command": "<exact shell command>",
  "explanation": "<one-line description of what it does>",
  "safe": true
}

Set "safe" to false if the command is destructive (rm, dd, mkfs, chmod 777 recursively, etc.).
If the query cannot be mapped to a Linux command, return {"command":"","explanation":"<reason>","safe":true}.
"""

# ─── MCP Tools Registry ────────────────────────────────────────────────────────
TOOLS = [
    {
        "name": "nlp_to_command",
        "description": "Translate a natural language query into a Linux shell command using AI.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "query": {
                    "type": "string",
                    "description": "Natural language description, e.g. 'show disk usage'"
                }
            },
            "required": ["query"]
        }
    },
    {
        "name": "execute_command",
        "description": "Execute a Linux shell command and return stdout/stderr/returncode.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "command": {"type": "string", "description": "Shell command to run"},
                "timeout":  {"type": "integer", "description": "Timeout in seconds (default 10)", "default": 10}
            },
            "required": ["command"]
        }
    },
    {
        "name": "nlp_execute",
        "description": "One-shot: translate NLP query → Linux command → execute it (safe commands only).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "query": {"type": "string", "description": "Natural language query"},
                "force": {"type": "boolean", "description": "Execute even if unsafe", "default": False}
            },
            "required": ["query"]
        }
    }
]

# ─── Tool Implementations ──────────────────────────────────────────────────────

def tool_nlp_to_command(query: str) -> dict:
    """Call Groq API and parse the JSON response."""
    if not GROQ_API_KEY:
        return {"error": "GROQ_API_KEY not set. Export it: export GROQ_API_KEY=gsk_..."}

    payload = {
        "model": GROQ_MODEL,
        "messages": [
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user",   "content": query}
        ],
        "temperature": 0.1,
        "max_tokens": 256
    }
    headers = {
        "Authorization": f"Bearer {GROQ_API_KEY}",
        "Content-Type": "application/json"
    }

    try:
        resp = requests.post(GROQ_URL, json=payload, headers=headers, timeout=15)
        resp.raise_for_status()
        raw = resp.json()["choices"][0]["message"]["content"].strip()

        # Strip markdown fences if Groq wraps in ```json
        if raw.startswith("```"):
            raw = raw.split("\n", 1)[1].rsplit("```", 1)[0].strip()

        return json.loads(raw)

    except json.JSONDecodeError:
        return {"error": "Could not parse model response as JSON", "raw": raw}
    except requests.HTTPError as e:
        return {"error": f"Groq API error: {e.response.status_code} {e.response.text}"}
    except Exception as e:
        return {"error": str(e)}


def tool_execute_command(command: str, timeout: int = 10) -> dict:
    """Run a shell command and return its output."""
    if not command.strip():
        return {"error": "Empty command"}
    try:
        result = subprocess.run(
            command, shell=True, capture_output=True, text=True,
            timeout=timeout, cwd=os.path.expanduser("~")
        )
        return {
            "command":    command,
            "stdout":     result.stdout,
            "stderr":     result.stderr,
            "returncode": result.returncode,
            "success":    result.returncode == 0
        }
    except subprocess.TimeoutExpired:
        return {"error": f"Command timed out after {timeout}s", "command": command}
    except Exception as e:
        return {"error": str(e)}


def tool_nlp_execute(query: str, force: bool = False) -> dict:
    """Translate then conditionally execute."""
    parsed = tool_nlp_to_command(query)

    if "error" in parsed:
        return parsed

    command     = parsed.get("command", "")
    is_safe     = parsed.get("safe", False)
    explanation = parsed.get("explanation", "")

    if not command:
        return {"query": query, "error": "Model could not generate a command", "explanation": explanation}

    if not is_safe and not force:
        return {
            "query":       query,
            "command":     command,
            "explanation": explanation,
            "safe":        False,
            "executed":    False,
            "message":     "Command flagged as unsafe. Set force=true to override."
        }

    exec_result = tool_execute_command(command)
    return {
        "query":       query,
        "command":     command,
        "explanation": explanation,
        "safe":        is_safe,
        "executed":    True,
        **exec_result
    }

# ─── JSON-RPC Dispatcher ───────────────────────────────────────────────────────

def dispatch(rpc: dict) -> dict:
    """Handle a single JSON-RPC 2.0 request and return a response dict."""
    req_id  = rpc.get("id")
    method  = rpc.get("method", "")
    params  = rpc.get("params", {})

    def ok(result):
        return {"jsonrpc": "2.0", "id": req_id, "result": result}

    def err(code, message):
        return {"jsonrpc": "2.0", "id": req_id, "error": {"code": code, "message": message}}

    # ── MCP Lifecycle ──────────────────────────────────────────────────────────
    if method == "initialize":
        return ok({
            "protocolVersion": "2024-11-05",
            "serverInfo":      {"name": "nlp-shell-mcp", "version": "1.0.0"},
            "capabilities":    {"tools": {}}
        })

    if method == "notifications/initialized":
        return None   # notification — no response needed

    # ── Tools ──────────────────────────────────────────────────────────────────
    if method == "tools/list":
        return ok({"tools": TOOLS})

    if method == "tools/call":
        tool_name = params.get("name", "")
        args      = params.get("arguments", {})

        if tool_name == "nlp_to_command":
            result = tool_nlp_to_command(args.get("query", ""))
        elif tool_name == "execute_command":
            result = tool_execute_command(args.get("command", ""), args.get("timeout", 10))
        elif tool_name == "nlp_execute":
            result = tool_nlp_execute(args.get("query", ""), args.get("force", False))
        else:
            return err(-32601, f"Unknown tool: {tool_name}")

        return ok({
            "content": [{"type": "text", "text": json.dumps(result, indent=2)}]
        })

    return err(-32601, f"Method not found: {method}")

# ─── HTTP Handler ──────────────────────────────────────────────────────────────

class MCPHandler(BaseHTTPRequestHandler):

    def log_message(self, fmt, *args):
        print(f"[MCP] {self.address_string()} — {fmt % args}")

    def send_json(self, data, status=200):
        body = json.dumps(data).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self):
        """CORS preflight."""
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, Accept")
        self.end_headers()

    def do_GET(self):
        """Health-check endpoint."""
        if self.path in ("/", "/health"):
            self.send_json({
                "status":  "ok",
                "server":  "nlp-shell-mcp",
                "version": "1.0.0",
                "transport": "Streamable HTTP",
                "endpoint": f"http://localhost:{PORT}/mcp",
                "tools":   [t["name"] for t in TOOLS]
            })
        else:
            self.send_json({"error": "Not found"}, 404)

    def do_POST(self):
        """Main MCP endpoint — accepts JSON-RPC 2.0."""
        if self.path != "/mcp":
            self.send_json({"error": "Not found. Use POST /mcp"}, 404)
            return

        length = int(self.headers.get("Content-Length", 0))
        body   = self.rfile.read(length)

        try:
            rpc = json.loads(body)
        except json.JSONDecodeError:
            self.send_json(
                {"jsonrpc": "2.0", "id": None,
                 "error": {"code": -32700, "message": "Parse error"}},
                400
            )
            return

        response = dispatch(rpc)

        if response is None:
            # It was a notification — return 202 with no body
            self.send_response(202)
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
        else:
            self.send_json(response)

# ─── Entry Point ───────────────────────────────────────────────────────────────

if __name__ == "__main__":
    if not GROQ_API_KEY:
        print("⚠  WARNING: GROQ_API_KEY is not set.")
        print("   Get a free key at https://console.groq.com")
        print("   Then: export GROQ_API_KEY=gsk_...\n")

    server = HTTPServer(("0.0.0.0", PORT), MCPHandler)
    print(f"✓  NLP Shell MCP Server running")
    print(f"   Endpoint : http://localhost:{PORT}/mcp")
    print(f"   Health   : http://localhost:{PORT}/health")
    print(f"   Model    : {GROQ_MODEL} via Groq (free)")
    print(f"   Press Ctrl+C to stop\n")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n✗  Server stopped.")
