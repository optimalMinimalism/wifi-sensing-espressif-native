# Piano di test — ESP32-S3 Wi-Fi sensing

## Obiettivo e baseline

Verificare separatamente affidabilità del collegamento, validità della calibrazione,
riconoscimento di `EMPTY`, `PRESENCE` e `MOTION`, e recupero dai guasti. La build
locale con ESP-IDF 6.0 passa; i test sul campo richiedono ESP32-S3 e router 2,4 GHz.

Prima delle prove annotare: commit, versione ESP-IDF, versione del componente,
modello router, SSID/BSSID, canale, distanza e posizione di scheda e router,
numero di persone, ostacoli, e valori dei parametri. Usare la stessa posizione
e scena in tutti i confronti. Il `sdkconfig` generato non è versionato: salvare
nel registro prove i parametri effettivi di ogni firmware.

Configurazione iniziale attuale: calibrazione 15 s, lettura applicativa 500 ms,
ping 100 Hz, conferma 2 campioni, filtro ACTIVE 300 ms, sensibilità movimento
0,500, minimo jitter ACTIVE 0,010, sensibilità presenza 0,250. I valori Kconfig
per le sensibilità sono moltiplicati per 1000.

## Preparazione

1. Inserire SSID e password del router 2,4 GHz in `main/config.h`. Evitare di
   pubblicare questo file se contiene credenziali reali.
2. Compilare con ESP-IDF 6.0: `idf.py build`.
3. Con la scheda collegata: `idf.py -p /dev/ttyACM0 flash monitor`, adattando
   la porta. Salvare il log seriale con i timestamp.
4. Per «zona monitorata» intendere inizialmente l'area fra router ed ESP32-S3
   e nelle loro vicinanze, inclusi i punti che possono riflettere il segnale.
   Non esiste un raggio fisso centrato su uno dei due dispositivi. Durante la
   prima calibrazione tenere persone e animali fuori da questa zona; se si
   vuole monitorare una stanza, la scelta più semplice è lasciarla vuota.
   Iniziare **prima del reset** e continuare fino a `calibration completed`.
   Riavviare per ogni nuova calibrazione.
5. Definire prima delle prove un evento osservabile: per esempio una persona
   attraversa la linea tra router e scheda; per presenza resta ferma nel punto
   segnato sul pavimento. Un osservatore annota l'istante reale di inizio/fine.

Il firmware registra soltanto i cambi di stato, non ogni campione. Per le
misure di latenza e per il tuning fine servirà aggiungere una modalità diagnostica
che registri periodicamente stato, `jitter`, `wander`, `calibrated` e i campi
diagnostici del componente (`init_stage`, `presence_ready`, soglie e medie).
Non usare l'assenza di nuove righe nel log come prova che il rilevamento sia
fermato: può significare che lo stato non è cambiato.

## Batteria iniziale

| ID | Prova | Procedura | Evidenza / criterio iniziale |
| --- | --- | --- | --- |
| B1 | Build e avvio | Compilare, flashare, resettare con area vuota. | Build riuscita; `connected`, `calibration started`, `calibration completed`, `sensing started` nell'ordine; nessun reset o errore. |
| B2 | Stabilità a vuoto | Lasciare la scena vuota 10 min dopo l'avvio. Ripetere 3 volte. | Annotare tempo fino a `EMPTY`, minuti in `EMPTY` e falsi `PRESENCE`/`MOTION`. Obiettivo provvisorio: al massimo 1 falso evento ogni 10 min. |
| B3 | Movimento | 10 attraversamenti identici, separati da almeno 10 s di scena vuota. | Contare rilevazioni `MOTION`, falsi negativi e latenza dal passaggio al log. Obiettivo provvisorio: almeno 9/10 rilevati entro 3 s. |
| B4 | Presenza ferma | 10 ingressi; fermarsi 60 s nello stesso punto, poi uscire. | Contare episodi con `PRESENCE` e tempo per tornare a `EMPTY`. Registrare anche se l'ingresso produce `MOTION` prima di `PRESENCE`. |
| B5 | Disturbi | Ripetere B2 con porte, ventilatore, animali o traffico nella stanza adiacente, uno alla volta. | Confrontare i falsi eventi con B2; annotare il disturbo preciso. |
| B6 | Calibrazione contaminata | Ripetere l'avvio con una persona nella scena; poi ripetere con scena vuota. | Confrontare B2–B4. Questa prova misura quanto la baseline dipende dalla scena, non va usata come impostazione finale. |
| R1 | Router assente all'avvio | Spegnere l'AP, riavviare la scheda, riaccendere l'AP dopo il timeout di 30 s. | Atteso dal codice: timeout e uscita da `app_main`; documentare che non riparte da solo dopo il timeout iniziale. |
| R2 | Disconnessione durante rilevamento | Spegnere e riaccendere il router mantenendo lo stesso BSSID. | Una sola segnalazione continua di lettura non disponibile; verificare ritorno a stati validi dopo la riconnessione. |
| R3 | Cambio BSSID | Se disponibili due AP con stesso SSID, forzare il roaming. | Atteso dal codice: lettura in errore `DRIVER_INVALID_DATA`; riavvio e nuova calibrazione necessari. |
| R4 | Tenuta | Lasciare acceso 8–24 h con eventi di movimento annotati. | Nessun reset, blocco o lunga sequenza `UNKNOWN`; confrontare falsi eventi e tempi di risposta tra inizio e fine. |

I criteri numerici sono **obiettivi iniziali da confermare** in base all'uso
desiderato. Non sono garanzie del componente. Per B3/B4 riportare sempre il
denominatore e non soltanto la percentuale.

## Registro prove

Una riga per evento o finestra di osservazione, con colonne:

`run_id,firmware_commit,config_id,router_bssid,canale,distanza_m,scenario,replica,t_reale_inizio,t_reale_fine,t_log_stato,stato,esito,note`

Tenere a parte i due valori stampati dopo la calibrazione (`wander` e `jitter`)
per ogni `run_id`. Per analizzare B2/B5: falsi eventi per ora. Per B3/B4:
successi su 10 e distribuzione delle latenze, includendo i mancati rilevamenti.

## Come fare tuning

1. Eseguire B1–B4 con i valori iniziali. Prima di cambiare configurazione,
   escludere problemi di posizionamento, BSSID variabile, calibrazione non vuota
   o campioni CSI insufficienti.
2. Se il movimento manca spesso, aumentare gradualmente
   `CONFIG_ESP_WIFI_SENSING_DEFAULT_SENSITIVITY` (per esempio 500 → 600).
   Se ci sono falsi `MOTION`, ridurla oppure alzare
   `CONFIG_ESP_WIFI_SENSING_DEFAULT_ACTIVE_JITTER_MIN` (10 → 20). La modifica
   della sensibilità movimento fa riapprendere la baseline del componente.
3. Se la presenza ferma manca, aumentare
   `CONFIG_ESP_WIFI_SENSING_DEFAULT_PRESENCE_SENSITIVITY` (250 → 350); se
   segnala persone a scena vuota, ridurla. È indipendente dalla sensibilità
   movimento.
4. Se lo stato `MOTION` oscilla rapidamente, aumentare
   `CONFIG_ESP_WIFI_SENSING_CONFIRM_COUNT` o
   `CONFIG_ESP_WIFI_SENSING_ACTIVE_FILTER_MS`. Questo può aumentare la latenza
   e far perdere movimenti brevi: ripetere B3 dopo ogni modifica.
5. Toccare `CONFIG_ESP_WIFI_SENSING_PING_FREQUENCY_HZ` solo se i diagnostici
   indicano campionamento insufficiente oppure se traffico/CPU sono un problema.
   Valori maggiori aumentano traffico e carico.
6. Se la calibrazione varia molto tra avvii, provare 15 → 30 s per
   `SENSING_CALIBRATION_MS`, mantenendo la scena vuota. Ripetere B2–B4.
7. Cambiare **un solo parametro per firmware**, registrare il valore in
   `sdkconfig.defaults` quando deciso e rieseguire B2–B4 nella stessa scena.
   `sdkconfig` è il file locale effettivo: dopo aver cambiato i defaults,
   verificare che il valore generato corrisponda prima di interpretare i test.

## Limiti da tenere presenti

- Il campionamento dell'applicazione ogni 500 ms può non osservare stati
  `MOTION` molto brevi, anche quando il componente li ha prodotti.
- `EMPTY` può essere emesso quando la calibrazione è valida e la fase di
  inizializzazione è stabile, anche se `presence_ready` non è ancora vero:
  verificare questo campo nella modalità diagnostica prima di giudicare la
  precisione della presenza.
- La riconnessione automatica Wi-Fi è prevista dopo una disconnessione in
  esercizio; il timeout della prima connessione ferma invece l'avvio.
- Il peer di sensing è il BSSID dell'AP al momento dell'inizializzazione:
  il roaming richiede una nuova calibrazione nella versione attuale.
