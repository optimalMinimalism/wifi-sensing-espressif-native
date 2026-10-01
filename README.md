# ESP32-S3 Wi-Fi sensing PoC

This ESP-IDF project uses one ESP32-S3 and a 2.4 GHz Wi-Fi router to classify
`EMPTY`, `PRESENCE`, and `MOTION` from Espressif's Wi-Fi CSI sensing component.
The station waits for an IP address and normally uses the connected AP BSSID as
its single sensing peer, pinging the router to keep CSI samples flowing. It can
instead use the fixed MAC of the option-B dedicated transmitter; in that mode
router pings are disabled. Startup trains against an empty monitored area for at
least 30 seconds, waiting up to 60 seconds for usable training data before
starting detection.
A FreeRTOS task reads the sensing state every 500 ms and logs state changes and
periodic diagnostics.

## Before flashing

1. Copy `.env.example` to `.env` and set `WIFI_SSID` and `WIFI_PASSWORD` there. Use `WIFI_SSID=<network name>` and `WIFI_PASSWORD=<password>`; spaces inside `<...>` belong to the value. For an open network, use `WIFI_PASSWORD=<>`. `.env` stays local; the build embeds these values in the firmware. `SENSING_PEER_MAC=<>` keeps router sensing; setting it to the option-B transmitter MAC selects the dedicated source and disables router pings. See `transmitter/option-b-dedicated-tx/README.md`.
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
the sensing task reports a read error in router mode; reboot to calibrate for the
new AP. In dedicated-transmitter mode, the new AP must still use the same channel
as the transmitter.

## Presence experiment (1 October 2026)

The empty-room capture `logs/idf-20261001-112349-7305.txt` reported `PRESENCE`
continuously from 11:24:44 until movement at 11:35:12. The component's default
presence sensitivity of 0.25 multiplies the trained wander threshold by 8.25
before comparing it with the current wander average. In that capture the
effective threshold was about 0.000108, below the empty-room readings.

The application now uses an experimental presence threshold of 2.5 times the
trained wander threshold (`SENSING_PRESENCE_THRESHOLD_MULTIPLIER` in
`main/config.h`). Motion detection still comes from the component. Diagnostics
show `presence_threshold` (component threshold), `applied_threshold` (application
threshold), and `native_presence` so both decisions can be compared. A replay of
482 logged empty-room samples from 11:24:50 to 11:35:00 found none above the
2.5x threshold. The afternoon runs used the previous 3x setting and showed
substantial overlap between labeled occupied and outside periods. The 2.5x
setting is an offline-selected experiment, not a new physical validation. The four
template samples reported by that training run are distinct from its 588
background points and do not by themselves establish a calibration failure.

After flashing, keep the room empty through calibration and record several
minutes of empty-room output, followed by timed entries and stationary holds at
A and B, exits, and a separate period with movement outside the room. Record
each change to the second. Check whether `EMPTY` holds during the empty periods
and whether the stationary holds reliably produce `PRESENCE`. Tune the multiplier
only after comparing both kinds of errors; the old logs do not provide precise
enough labels to establish A/B accuracy.
