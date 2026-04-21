#include <stdio.h>      // snprintf
#include <stdlib.h>     // malloc, free, realloc
#include <string.h>     // strlen, strstr, strchr, strncpy, memcpy
#include <curl/curl.h>  // libcurl
#include "../../include/core/shell.h"

int is_nlp_query(struct source_s *src)
{
    if (!src || !src->buffer) return 0;

    // skip leading spaces
    char *p = src->buffer;
    while (*p == ' ' || *p == '\t') p++;

    return (p[0] == ':' && p[1] == ':'); // "::"
}

char *extract_query(struct source_s *src)
{
    char *p = src->buffer;

    while (*p == ' ' || *p == '\t') p++;

    if (p[0] == ':' && p[1] == ':')
        p += 2;

    while (*p == ' ') p++;

    return safe_strdup(p); // rest is query
}

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t total = size * nmemb;
    struct memory *mem = (struct memory *)userp;

    char *ptr = realloc(mem->data, mem->size + total + 1);
    if (!ptr) return 0;

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, total);
    mem->size += total;
    mem->data[mem->size] = 0;

    return total;
}

char *call_mcp(const char *query)
{
    CURL *curl;
    CURLcode res;
    char *escaped = escape_json(query);
    struct memory chunk = {0};

    curl = curl_easy_init();
    if (!curl) return NULL;

    char post_data[1024];

    snprintf(post_data, sizeof(post_data),
        "{"
        "\"jsonrpc\":\"2.0\","
        "\"id\":1,"
        "\"method\":\"tools/call\","
        "\"params\":{"
            "\"name\":\"nlp_to_command\","
            "\"arguments\":{"
                "\"query\":\"%s\""
            "}"
        "}"
        "}", escaped);
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8080/mcp");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    res = curl_easy_perform(curl);

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK)
        return NULL;

    free(escaped);
    return chunk.data;
}

char *extract_command(const char *response)
{
    if (!response) return NULL;

    // 🔹 Step 1: find "text"
    char *text = strstr(response, "\"text\":");
    if (!text) return NULL;

    // 🔹 Step 2: find start of string content
    char *start = strchr(text + 7, '\"'); // after "text":
    if (!start) return NULL;

    start++; // move inside string

    // 🔹 Step 3: unescape into buffer
    char *buf = malloc(4096);
    int j = 0;

    for (int i = 0; start[i]; i++)
    {
        if (start[i] == '\\')
        {
            i++;
            if (start[i] == 'n') buf[j++] = '\n';
            else if (start[i] == '\"') buf[j++] = '\"';
            else if (start[i] == '\\') buf[j++] = '\\';
            else buf[j++] = start[i];
        }
        else if (start[i] == '\"')
        {
            break;
        }
        else
        {
            buf[j++] = start[i];
        }
    }

    buf[j] = '\0';

    // DEBUG (VERY IMPORTANT)
    //printf("[DEBUG] CLEAN JSON:\n%s\n", buf);

    // Step 4: extract command
    char *cmd_key = strstr(buf, "\"command\"");
    if (!cmd_key)
    {
        free(buf);
        return NULL;
    }

    char *cmd_start = strchr(cmd_key, ':');
    cmd_start = strchr(cmd_start, '\"');
    cmd_start++;

    char *cmd_end = strchr(cmd_start, '\"');
    if (!cmd_end)
    {
        free(buf);
        return NULL;
    }

    size_t len = cmd_end - cmd_start;

    char *cmd = malloc(len + 1);
    strncpy(cmd, cmd_start, len);
    cmd[len] = '\0';

    free(buf);
    printf("[CMD] : %s\n", cmd);
    return cmd;
}

char *handle_nlp(struct source_s *src)
{
    char *query = extract_query(src);
    if (!query) return NULL;

    char *response = call_mcp(query);
    free(query);

    if (!response) return NULL;

    char *cmd = extract_command(response);
    free(response);

    return cmd;
}

char *safe_strdup(const char *s)
{
    if (!s) return NULL;
    size_t len = strlen(s);

    char *dup = malloc(len + 1);
    if (!dup) return NULL;

    memcpy(dup, s, len + 1);
    return dup;
}

char *escape_json(const char *input)
{
    size_t len = strlen(input);
    char *out = malloc(len * 2 + 1); // safe upper bound
    char *p = out;

    for (size_t i = 0; i < len; i++)
    {
        switch (input[i])
        {
            case '\"': *p++ = '\\'; *p++ = '\"'; break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\n': *p++ = '\\'; *p++ = 'n';  break;
            case '\r': *p++ = '\\'; *p++ = 'r';  break;
            case '\t': *p++ = '\\'; *p++ = 't';  break;
            default:   *p++ = input[i];
        }
    }

    *p = '\0';
    return out;
}