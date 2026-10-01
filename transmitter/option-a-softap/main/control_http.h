#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    char wifi[24];
    char ip[16];
    unsigned clients;
    bool has_sensing;
    bool calibrated;
    char sensing_state[24];
    float wander_raw;
    float presence_avg;
    float presence_threshold;
} control_http_snapshot_t;

typedef void (*control_http_snapshot_fn)(control_http_snapshot_t *snapshot, void *context);

typedef struct {
    const char *role;
    const char *token;
    uint16_t port;
    control_http_snapshot_fn snapshot;
    void *context;
} control_http_config_t;

// Start once per board after its IP interface is ready. Token must remain valid
// until stop and is required on every request as the X-Control-Token header.
esp_err_t control_http_start(const control_http_config_t *config);
esp_err_t control_http_stop(void);

// Publish a short, structured line for remote monitoring. Safe from tasks and
// Wi-Fi event callbacks; a bounded ring retains the most recent 64 lines.
void control_http_publish(const char *type, const char *message);
