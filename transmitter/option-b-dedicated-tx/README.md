# Opzione B — trasmettitore CSI dedicato

## Obiettivo

Separare la sorgente dei pacchetti dal ricevitore CSI: una scheda trasmette a cadenza controllata, una ESP32-S3 riceve e classifica. Valutare il risultato con le stesse scene etichettate dell'opzione A.

## Configurazione prevista

| Elemento | Configurazione prevista |
| --- | --- |
| Scheda A | Trasmettitore di pacchetti su canale e frequenza fissati. |
| Scheda B | ESP32-S3 che acquisisce il CSI dei pacchetti di A e registra diagnostici e stati. |
| Prova | Distanza, altezza, orientamento, canale, frequenza e alimentazione fissati e annotati. |

Gli esempi ufficiali Espressif [`csi_send`/`csi_recv`](https://github.com/espressif/esp-csi/tree/master/examples/get-started) mostrano l'acquisizione fra due schede. Il nostro firmware, invece, usa il BSSID dell'AP connesso e pinga il gateway: la selezione di A come sorgente CSI richiede integrazione e verifica.

## Milestone

1. **B1 — Collegamento radio:** ricevere pacchetti da A e identificare il suo MAC nei campioni CSI; misurare la continuità dei campioni.
2. **B2 — Sensing:** integrare la sorgente A con il componente di sensing; verificare training, soglie e diagnostici prima di giudicare gli stati.
3. **B3 — Rete di gestione:** definire come dare un IP ad A e B per il controllo remoto. Un collegamento di soli pacchetti CSI/ESP-NOW non fornisce, da solo, endpoint HTTP. Se si usa SoftAP o STA in parallelo, verificare canale condiviso e possibile effetto del traffico di gestione sul CSI.
4. **B4 — Controllo remoto:** se serve dopo i test iniziali, implementare HTTP nel firmware B dedicato. Trasmettitore e receiver restano firmware separati; ciascuno espone solo i propri endpoint quando ha un IP.
5. **B5 — Valutazione:** ripetere le prove etichettate vuoto, ingresso, persona ferma e uscita; confrontare con A per sessione.

**Stato:** piano; firmware trasmettitore, integrazione del ricevitore, rete di gestione e HTTP dedicato non ancora implementati.
