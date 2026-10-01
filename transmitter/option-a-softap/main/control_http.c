#include "control_http.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#define EVENT_CAPACITY 64
#define EVENT_TYPE_SIZE 20
#define EVENT_MESSAGE_SIZE 96

static const char *tag = "CONTROL_HTTP";

typedef struct {
    uint64_t id;
    int64_t uptime_ms;
    char type[EVENT_TYPE_SIZE];
    char message[EVENT_MESSAGE_SIZE];
} control_event_t;

static portMUX_TYPE event_mux = portMUX_INITIALIZER_UNLOCKED;
static control_event_t events[EVENT_CAPACITY];
static uint64_t next_id = 1;
static size_t event_count;
static httpd_handle_t server;
static esp_timer_handle_t reboot_timer;
static control_http_config_t active_config;

static void append_escaped(char *output, size_t capacity, const char *input)
{
    size_t written = 0;
    if (capacity == 0) return;
    for (; *input && written + 1 < capacity; ++input) {
        const char *replacement = NULL;
        char escaped[7];
        switch (*input) {
        case '"': replacement = "\\\""; break;
        case '\\': replacement = "\\\\"; break;
        case '\n': replacement = "\\n"; break;
        case '\r': replacement = "\\r"; break;
        case '\t': replacement = "\\t"; break;
        default:
            if ((unsigned char)*input < 0x20) {
                snprintf(escaped, sizeof(escaped), "\\u%04x", (unsigned char)*input);
                replacement = escaped;
            }
            break;
        }
        if (replacement != NULL) {
            size_t length = strlen(replacement);
            if (written + length >= capacity) break;
            memcpy(output + written, replacement, length);
            written += length;
        } else {
            output[written++] = *input;
        }
    }
    output[written] = '\0';
}

void control_http_publish(const char *type, const char *message)
{
    if (type == NULL || message == NULL) return;
    control_event_t item = {
        .uptime_ms = esp_timer_get_time() / 1000,
    };
    snprintf(item.type, sizeof(item.type), "%s", type);
    snprintf(item.message, sizeof(item.message), "%s", message);
    portENTER_CRITICAL(&event_mux);
    item.id = next_id++;
    events[(item.id - 1) % EVENT_CAPACITY] = item;
    if (event_count < EVENT_CAPACITY) ++event_count;
    portEXIT_CRITICAL(&event_mux);
}

static bool authorized(httpd_req_t *request)
{
    size_t length = httpd_req_get_hdr_value_len(request, "X-Control-Token");
    size_t expected = strlen(active_config.token);
    if (length != expected || length == 0 || length >= 128) {
        httpd_resp_send_err(request, HTTPD_401_UNAUTHORIZED, "X-Control-Token required");
        return false;
    }
    char provided[128];
    if (httpd_req_get_hdr_value_str(request, "X-Control-Token", provided,
                                    sizeof(provided)) != ESP_OK) {
        httpd_resp_send_err(request, HTTPD_401_UNAUTHORIZED, "X-Control-Token required");
        return false;
    }
    unsigned difference = 0;
    for (size_t i = 0; i < expected; ++i) {
        difference |= (unsigned char)provided[i] ^ (unsigned char)active_config.token[i];
    }
    if (difference != 0 || provided[expected] != '\0') {
        httpd_resp_send_err(request, HTTPD_401_UNAUTHORIZED, "Invalid token");
        return false;
    }
    return true;
}

extern const char index_html_start[] asm("_binary_index_html_start");

static esp_err_t page_handler(httpd_req_t *request)
{
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, index_html_start, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_handler(httpd_req_t *request)
{
    if (!authorized(request)) return ESP_OK;
    control_http_snapshot_t snapshot = {0};
    active_config.snapshot(&snapshot, active_config.context);
    char role[48], wifi[48], ip[32], state[48];
    append_escaped(role, sizeof(role), active_config.role);
    append_escaped(wifi, sizeof(wifi), snapshot.wifi);
    append_escaped(ip, sizeof(ip), snapshot.ip);
    append_escaped(state, sizeof(state), snapshot.sensing_state);
    char body[512];
    int length = snprintf(body, sizeof(body),
                          "{\"role\":\"%s\",\"uptime_ms\":%" PRId64
                          ",\"wifi\":\"%s\",\"ip\":\"%s\",\"clients\":%u,"
                          "\"sensing\":",
                          role, esp_timer_get_time() / 1000, wifi, ip, snapshot.clients);
    if (length < 0 || (size_t)length >= sizeof(body)) return ESP_FAIL;
    int extra;
    if (snapshot.has_sensing) {
        extra = snprintf(body + length, sizeof(body) - length,
                         "{\"state\":\"%s\",\"calibrated\":%s,"
                         "\"wander_raw\":%.9f,\"presence_avg\":%.9f,"
                         "\"presence_threshold\":%.9f}}",
                         state, snapshot.calibrated ? "true" : "false",
                         snapshot.wander_raw, snapshot.presence_avg,
                         snapshot.presence_threshold);
    } else {
        extra = snprintf(body + length, sizeof(body) - length, "null}");
    }
    if (extra < 0 || (size_t)extra >= sizeof(body) - length) return ESP_FAIL;
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, body, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t events_handler(httpd_req_t *request)
{
    if (!authorized(request)) return ESP_OK;
    uint64_t after = 0;
    size_t query_length = httpd_req_get_url_query_len(request);
    if (query_length > 0) {
        if (query_length >= 48) {
            httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid query");
            return ESP_OK;
        }
        char query[48], value[32];
        if (httpd_req_get_url_query_str(request, query, sizeof(query)) != ESP_OK ||
            httpd_query_key_value(query, "after", value, sizeof(value)) != ESP_OK ||
            value[0] == '-' || value[0] == '\0') {
            httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Expected after=<id>");
            return ESP_OK;
        }
        char *end = NULL;
        after = strtoull(value, &end, 10);
        if (*end != '\0') {
            httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid event ID");
            return ESP_OK;
        }
    }
    uint64_t first, latest;
    portENTER_CRITICAL(&event_mux);
    latest = next_id - 1;
    first = next_id - event_count;
    portEXIT_CRITICAL(&event_mux);
    char chunk[1024];
    snprintf(chunk, sizeof(chunk),
             "{\"first_available_id\":%" PRIu64 ",\"latest_id\":%" PRIu64
             ",\"events\":[", first, latest);
    httpd_resp_set_type(request, "application/json");
    esp_err_t error = httpd_resp_send_chunk(request, chunk, HTTPD_RESP_USE_STRLEN);
    bool sent = false;
    for (uint64_t id = first; error == ESP_OK && id <= latest; ++id) {
        if (id <= after) continue;
        control_event_t item;
        portENTER_CRITICAL(&event_mux);
        item = events[(id - 1) % EVENT_CAPACITY];
        portEXIT_CRITICAL(&event_mux);
        if (item.id != id) continue;
        char type[EVENT_TYPE_SIZE * 6 + 1], message[EVENT_MESSAGE_SIZE * 6 + 1];
        append_escaped(type, sizeof(type), item.type);
        append_escaped(message, sizeof(message), item.message);
        int length = snprintf(chunk, sizeof(chunk),
                              "%s{\"id\":%" PRIu64 ",\"uptime_ms\":%" PRId64
                              ",\"type\":\"%s\",\"message\":\"%s\"}",
                              sent ? "," : "", item.id, item.uptime_ms, type, message);
        if (length < 0 || (size_t)length >= sizeof(chunk)) return ESP_FAIL;
        error = httpd_resp_send_chunk(request, chunk, length);
        sent = true;
    }
    if (error == ESP_OK) error = httpd_resp_send_chunk(request, "]}", 2);
    if (error == ESP_OK) error = httpd_resp_send_chunk(request, NULL, 0);
    return error;
}

static void reboot_callback(void *argument)
{
    (void)argument;
    esp_restart();
}

static esp_err_t reboot_handler(httpd_req_t *request)
{
    if (!authorized(request)) return ESP_OK;
    esp_err_t error = esp_timer_start_once(reboot_timer, 300000);
    if (error != ESP_OK) {
        httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Reboot unavailable");
        return ESP_OK;
    }
    control_http_publish("control", "reboot requested");
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, "{\"rebooting\":true}");
}

esp_err_t control_http_start(const control_http_config_t *config)
{
    if (config == NULL || config->role == NULL || config->token == NULL ||
        config->token[0] == '\0' || config->snapshot == NULL || server != NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    active_config = *config;
    esp_timer_create_args_t timer_args = {
        .callback = reboot_callback,
        .name = "http_reboot",
    };
    esp_err_t error = esp_timer_create(&timer_args, &reboot_timer);
    if (error != ESP_OK) return error;
    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    if (config->port != 0) http_config.server_port = config->port;
    http_config.max_uri_handlers = 4;
    error = httpd_start(&server, &http_config);
    if (error == ESP_OK) {
        httpd_uri_t page = {.uri = "/", .method = HTTP_GET, .handler = page_handler};
        httpd_uri_t status = {.uri = "/status", .method = HTTP_GET, .handler = status_handler};
        httpd_uri_t events_uri = {.uri = "/events", .method = HTTP_GET, .handler = events_handler};
        httpd_uri_t reboot = {.uri = "/reboot", .method = HTTP_POST, .handler = reboot_handler};
        error = httpd_register_uri_handler(server, &page);
        if (error == ESP_OK) error = httpd_register_uri_handler(server, &status);
        if (error == ESP_OK) error = httpd_register_uri_handler(server, &events_uri);
        if (error == ESP_OK) error = httpd_register_uri_handler(server, &reboot);
    }
    if (error != ESP_OK) {
        if (server != NULL) httpd_stop(server);
        server = NULL;
        esp_timer_delete(reboot_timer);
        reboot_timer = NULL;
        ESP_LOGE(tag, "start failed: %s", esp_err_to_name(error));
        return error;
    }
    ESP_LOGI(tag, "HTTP control ready on port %u", http_config.server_port);
    control_http_publish("control", "HTTP server ready");
    return ESP_OK;
}

esp_err_t control_http_stop(void)
{
    if (server == NULL) return ESP_ERR_INVALID_STATE;
    esp_err_t error = httpd_stop(server);
    server = NULL;
    esp_timer_stop(reboot_timer);
    esp_timer_delete(reboot_timer);
    reboot_timer = NULL;
    return error;
}
