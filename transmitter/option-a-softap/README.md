# Opzione A — due ESP32, una come SoftAP

## Obiettivo

Confrontare il sensing con modem e quello su un collegamento radio controllato fra due schede. La prova misura falsi allarmi e rilevazioni; non assume che il cambio di access point migliori il classificatore.

## Configurazione di partenza

| Elemento | Configurazione prevista |
| --- | --- |
| Scheda A | ESP32 in modalità SoftAP 2,4 GHz; firmware dedicato a rete, traffico di risposta e controllo HTTP. |
| Scheda B | ESP32-S3 con il firmware di sensing corrente, in modalità STA associata ad A. Usa il BSSID dell'AP come peer e pinga il gateway. |
| Rete | SSID e password locali; canale e larghezza fissati e annotati. A fornisce IP via DHCP; l'IP assegnato a B va registrato. Internet non è necessario per il sensing. |
| Geometria | Schede su supporti fissi; annotare distanza, altezza, orientamento delle antenne, ostacoli e posizione delle persone. |
| Alimentazione | Due alimentazioni stabili. Il PC può registrare la seriale di B, ma la sua posizione resta fissa durante i confronti. |

Il firmware attuale di B richiede modifiche alle credenziali `.env` per collegarsi alla rete di A. Non è ancora verificato su hardware che il SoftAP risponda ai ping con campioni CSI adatti a `esp_wifi_sensing`.

## Milestone e criteri di uscita

1. **A1 — SoftAP operativo:** A avvia la rete sul canale scelto; B ottiene un IP e registra il BSSID di A. Annotare firmware e configurazione di entrambe le schede.
2. **A2 — CSI e calibrazione:** B avvia i ping, accumula campioni di training, termina la calibrazione e registra `wander_raw`, `presence_avg`, soglia e stati per almeno 5 minuti. Se manca il CSI, fermarsi e verificare traffico/peer prima di valutare il rilevamento.
3. **A3 — Controllo remoto del transmitter:** verificare `GET /status`, `GET /events?after=<id>` e `POST /reboot` sul SoftAP A. Il receiver B mantiene il firmware di sensing corrente e il monitor seriale durante i primi test.
4. **A4 — Misure etichettate:** ripetere calibrazione indipendente, periodo vuoto, ingresso, persona ferma e uscita. Registrare orari reali, falsi eventi, latenza e tempo in `EMPTY`/`PRESENCE`.
5. **A5 — Confronto:** ripetere con il modem mantenendo fissi ricevitore e geometria per quanto possibile. Riportare risultati per sessione, non solo una media complessiva.

**Stato:** firmware SoftAP, HTTP e pagina browser compilati con ESP-IDF 6.0 per ESP32-S3; flash e verifica hardware ancora da fare. HTTP sul receiver rimandato a dopo i primi test.

## Firmware del SoftAP

Questa cartella è un progetto ESP-IDF separato. Il codice HTTP in `main/control_http.c` appartiene soltanto al transmitter SoftAP; il receiver usa il progetto nella root e non compila questo codice. Modificare il `.env` locale già creato (oppure copiare `.env.example` in una nuova installazione), impostare SSID/password/token/canale, quindi eseguire `idf.py build flash monitor` da questa cartella. Il SoftAP usa WPA2 e il server DHCP standard di ESP-IDF. La risposta ai ping e la disponibilità del CSI sul receiver vanno verificate su hardware.

## Aprire la pagina dal browser

1. Nel terminale, entrare in `transmitter/option-a-softap` e aprire `.env`. Il file locale contiene `AP_SSID`, `AP_PASSWORD` e `CONTROL_TOKEN` tra `< >`. I valori sono stati generati localmente e sono esclusi da Git; annotare password e token senza i delimitatori. Se modifichi `.env`, la build rigenera la configurazione.
2. Preparare ESP-IDF 6.0 (`. /home/optimalminimalist/.espressif/v6.0/esp-idf/export.sh`), poi dalla stessa cartella eseguire `idf.py -p /dev/ttyACM0 flash monitor`, sostituendo la porta con quella **della scheda transmitter**. Il receiver si flasha separatamente dal progetto nella root.
3. Collegare PC o telefono alla rete `AP_SSID` con `AP_PASSWORD`. Questa rete di prova non dà accesso a Internet; il dispositivo deve restare collegato al SoftAP.
4. Aprire `http://192.168.4.1/` nel browser. Inserire `CONTROL_TOKEN` e premere **Collega**. La pagina mostra online/offline, IP, numero di client, uptime ed eventi; **Riavvia SoftAP** invia il comando HTTP. Il token resta nella memoria della scheda del browser solo per questa sessione/tab.

L'IP effettivo viene stampato anche sulla seriale del transmitter. Il browser aggiorna stato ed eventi ogni 2 secondi; **Pausa** ferma le richieste durante le misure CSI. La pagina mostra soltanto eventi esplicitamente pubblicati dal transmitter (avvio, connessioni e riavvio), non l'intero flusso `ESP_LOG` e non gli stati del receiver. Il token viaggia in chiaro dentro HTTP sulla rete Wi-Fi di prova.

Per provare l'opzione A completa, configurare il `.env` del receiver nella root con lo stesso SSID e la stessa password del SoftAP, poi compilare e flashare i **due progetti su due schede diverse**. La risposta ai ping e la disponibilità del CSI sul receiver vanno verificate su hardware.
