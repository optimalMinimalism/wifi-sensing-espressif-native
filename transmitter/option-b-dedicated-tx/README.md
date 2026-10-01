# Opzione B — trasmettitore CSI dedicato

## Obiettivo

Separare la sorgente dei pacchetti dal ricevitore CSI: la scheda A trasmette
pacchetti ESP-NOW broadcast a cadenza controllata e la ESP32-S3 B usa il MAC di
A come unico peer del componente `esp_wifi_sensing`. Le due schede lavorano sullo
stesso canale 2,4 GHz e con larghezza HT20.

Questa implementazione copre il collegamento radio B1 e abilita i test di sensing
B2. Per i primi test A non ha un IP né endpoint HTTP; B resta collegata alla rete
Wi-Fi configurata nel progetto root, che ne determina il canale e può fornirle un
IP di gestione. Il traffico ping verso il router viene disabilitato quando B usa
il MAC dedicato. HTTP dedicato resta intenzionalmente fuori da questa milestone.

## Configurazione

| Elemento | Configurazione |
| --- | --- |
| Scheda A | Progetto in questa cartella; ESP-NOW broadcast, MAC, canale e frequenza fissi. |
| Scheda B | Progetto nella root; STA associata a un AP 2,4 GHz sullo stesso canale di A. |
| Canale | `TX_CHANNEL` deve coincidere con il canale reale dell'AP a cui si collega B. |
| Peer | `SENSING_PEER_MAC` di B deve coincidere esattamente con `TX_MAC` di A. |
| Geometria | Distanza, altezza, orientamento, alimentazione e posizione vanno fissati e annotati. |

Il MAC predefinito di esempio è locale e unicast (`1A:00:00:00:00:01`), così non
confligge con il MAC di fabbrica. Usare un valore diverso per ogni trasmettitore
attivo nello stesso ambiente.

## Preparazione e flash

1. Verificare il canale 2,4 GHz dell'AP usato dalla scheda B. Se l'AP sceglie il
   canale automaticamente, fissarlo nella configurazione del router prima della
   prova.
2. In questa cartella copiare `.env.example` in `.env` e impostare:

   ```text
   TX_CHANNEL=<6>
   TX_FREQUENCY_HZ=<100>
   TX_MAC=<1A:00:00:00:00:01>
   ```

3. Nel `.env` del progetto root mantenere SSID/password dell'AP e aggiungere lo
   stesso MAC:

   ```text
   SENSING_PEER_MAC=<1A:00:00:00:00:01>
   ```

   Per tornare alla modalità router dell'opzione originale usare
   `SENSING_PEER_MAC=<>`.
4. Compilare e flashare A con ESP-IDF 6.0:

   ```sh
   . /home/optimalminimalist/.espressif/v6.0/esp-idf/export.sh
   cd transmitter/option-b-dedicated-tx
   idf.py set-target esp32s3
   idf.py build
   idf.py -p /dev/ttyACM0 flash monitor
   ```

5. Compilare e flashare B separatamente dalla root. Non usare la stessa porta
   seriale per entrambe le schede.

Il log di A stampa MAC, canale, frequenza e, una volta al secondo, pacchetti
accettati o rifiutati dalla coda ESP-NOW. Un invio accettato conferma solo che il
driver ha preso in carico il frame broadcast, non che B lo abbia ricevuto.

## Criteri di verifica

1. Avviare A e controllare che il contatore `tx total` cresca e `rejected` resti
   normalmente a zero.
2. Avviare B con l'area vuota. Il log deve riportare il MAC dedicato e
   `router ping disabled`; durante la calibrazione `samples` e `background`
   devono crescere. Se restano a zero, controllare prima MAC e canale.
3. Solo dopo una calibrazione completa, registrare prove etichettate separate:
   vuoto, ingresso, persona ferma e uscita. Ripetere la calibrazione per ogni
   sessione e annotare geometria, canale, frequenza e alimentazione.
4. Confrontare i risultati per sessione con l'opzione A; non dedurre accuratezza
   dai soli contatori radio.

## Limiti attuali

- A è un trasmettitore radio senza rete IP o controllo HTTP.
- B usa un AP esistente come ancora di canale e rete di gestione; A e AP devono
  condividere il canale. Il traffico dell'AP può ancora influenzare il mezzo radio.
- ESP-NOW broadcast non fornisce acknowledgement end-to-end. La continuità reale
  si verifica dai campioni e dai diagnostici di B.
- Flash e validazione fisica sulle due schede restano necessari.

L'impostazione segue il modello degli esempi ufficiali Espressif
[`csi_send`/`csi_recv`](https://github.com/espressif/esp-csi/tree/master/examples/get-started),
adattato al componente `esp_wifi_sensing` già usato dal receiver.
