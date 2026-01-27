# Smart Fan

## Descrizione
Progetto che utilizza un Arduino UNO R3 come controllore di una piccola ventola che cerca di posizionarsi verso una persona vicina, e la velocità della cui ventola è dipendente dalla temperatura e umidità dell'aria, distanza dalla persona, e umidità della pelle di quest'ultima.

## Componenti Principali
* Elegoo UNO R3
* Breadboard
* Sensore ad ultrasuoni
* Sensore DHT11
* Sensore capacitivo di umidità v1.2
* L293D H-Bridge Motor Driver
* Motore a corrente continua da 3-6V
* ULN2003 Stepper Motor Driver
* Motore Stepper 28BYJ-48 da 5V DC
* Alimentatore da 6V
* Power Bank da 5V 8000 mAh (opzionale)

## Funzionamento
Quando alimentato, il ventilatore come prima cosa verifica che sia puntato verso qualcosa o qualcuno non più distante di mezzo metro, usanto il sensore ad ultrasuoni, quindi:

### Se è puntato verso un soggetto
1. Memorizza la distanza rilevata.
2. Raccoglie le letture dei sensori su temperatura e umidità dell'aria, e umidità della pelle del soggetto puntato.
3. Controlla che le letture siano nei loro range accettabili, e fa in modo che non influenzino il calcolo seguente se non lo sono.
4. Effettua una media pesata dei quattro fattori rilevati per ricavare un valore per la PWM che governa la velocità della ventola.
5. Se durante la ripetizione di questo loop non rileva più la presenza di un soggetto puntato, passa al comportamento descritto di seguito.

### Se non è puntato verso un soggetto
1. Effettua una breve rotazione parallela al piano d'appoggio, di pochi gradi in senso orario.
2. Invia un impulso ultrasonico per verificare che stia puntando verso un soggetto in seguito al movimento.
3. Se ha rilevato un soggetto sotto la distanza soglia, torna al comportamento normale descritto sopra per attivare la ventola.
4. Se non ha nuovamente rilevato un soggetto, ripete queste operazioni, invertendo il verso della rotazione e aumentandone via via l'ampiezza per coprire un'area più ampia.

### Inoltre
Se durante la normale gestione della ventola perde il contatto con il soggetto puntato, perché si è mosso o per errore del sensore, passa al suo comportmento "di ricerca" per cercare di riposizionarsi meglio.

## Considerazioni
Come possibili aggiunte sto valutando di aggiungere un pulsante di accensione/spegnimento per il ventilatore, per renderlo più realistico, e inoltre si potrebbero inserire dei valori di soglia minima per accendere la ventola solo qualora la temperatura dell'aria sia sufficientemente calda, e per verificare che la persona che stia usando il ventilatore stia sudando. Qualora il ventilatore sia acceso, ma le misurazioni cadano al di sotto dei valori storia, invece, si spegnerebbe da solo.

## Licenza
Questo progetto è rilasciato sotto licenza **GNU General Public License v3.0 **.
