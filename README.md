# ESP32-S3 Wi-Fi sensing PoC

This ESP-IDF project uses one ESP32-S3 and a 2.4 GHz Wi-Fi router to classify
`EMPTY`, `PRESENCE`, and `MOTION` from Espressif's Wi-Fi CSI sensing component.
The station waits for an IP address, uses the connected AP BSSID as its single
sensing peer, and pings the router to keep CSI samples flowing. Startup trains
against an empty monitored area for at least 30 seconds, waiting up to 60 seconds for usable training data before starting detection.
A FreeRTOS task logs only state changes every 500 ms.

## Before flashing

1. Copy `.env.example` to `.env` and set `WIFI_SSID` and `WIFI_PASSWORD` there. Use `WIFI_SSID=<network name>` and `WIFI_PASSWORD=<password>`; spaces inside `<...>` belong to the value. For an open network, use `WIFI_PASSWORD=<>`. `.env` stays local; the build embeds these values in the firmware.
2. Keep the area between the ESP32-S3 and router empty during boot calibration.
3. Use ESP-IDF 6.0 for this component release. This machine has it at
   `/home/optimalminimalist/.espressif/v6.0/esp-idf`; select that path in
   your IDE ESP-IDF extension as well. The project targets `esp32s3`.
   The published `esp_csi_gain_ctrl` and `esp_radar_motion_dec` binaries
   include ESP32-S3 builds for 6.0, but not 6.1.

```sh
. /home/optimalminimalist/.espressif/v6.0/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build
```

To flash, monitor, and save the terminal output locally, use:

```sh
./tools/idf-fm
```

The script creates a timestamped `logs/idf-*.txt` file in this project and
shows the same output in the terminal. `logs/` is ignored by Git. To run it
as `idf-fm` in the current shell, add `export PATH="$PWD/tools:$PATH"` while in
the project directory. With one connected serial device, the script selects
its port automatically; pass a port such as `/dev/ttyUSB0` if several exist,
or set `ESPPORT`. Exit the monitor with `Ctrl+]` as usual. Source ESP-IDF's
`export.sh` before running the script.

The first build fetches `espressif/esp_wifi_sensing` 0.1.1~3 and its managed
dependencies.

The sensing component's presence estimate depends on its calibration, CSI
sample quality, router traffic, and placement. After a disconnect, Wi-Fi
reconnects automatically. If the station reconnects to a different BSSID,
the sensing task reports a read error; reboot to calibrate for the new AP.
