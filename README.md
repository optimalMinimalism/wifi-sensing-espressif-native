# ESP32-S3 Wi-Fi sensing PoC

This ESP-IDF project uses one ESP32-S3 and a 2.4 GHz Wi-Fi router to classify
`EMPTY`, `PRESENCE`, and `MOTION` from Espressif's Wi-Fi CSI sensing component.
The station waits for an IP address, uses the connected AP BSSID as its single
sensing peer, and pings the router to keep CSI samples flowing. Startup trains
against an empty monitored area for 15 seconds before starting detection.
A FreeRTOS task logs only state changes every 500 ms.

## Before flashing

1. Copy `.env.example` to `.env` and set `WIFI_SSID` and `WIFI_PASSWORD` there. Use plain `KEY=value` lines without quotes. `.env` stays local; the build embeds these values in the firmware.
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
idf.py -p /dev/ttyACM0 flash monitor
```

Replace `/dev/ttyACM0` with your board's serial port. The first build
fetches `espressif/esp_wifi_sensing` 0.1.1~3 and its managed dependencies.

The sensing component's presence estimate depends on its calibration, CSI
sample quality, router traffic, and placement. After a disconnect, Wi-Fi
reconnects automatically. If the station reconnects to a different BSSID,
the sensing task reports a read error; reboot to calibrate for the new AP.
