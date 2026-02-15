# Smart Fan

## Descrizione
Progetto che utilizza un Arduino UNO R3 come controllore di una piccola ventola che cerca di posizionarsi verso una persona vicina, e la velocità della cui ventola è dipendente dalla temperatura e umidità dell'aria, distanza dalla persona, e umidità della pelle di quest'ultima.

## Componenti Principali
* Elegoo UNO R3
* Breadboard
* Sensore ad ultrasuoni
* Sensore DHT11
* Sensore capacitivo di umidità v1.2
* Pulsante
* L293D Motor Driver
* Motore a corrente continua da 3-6V
* ULN2003 Stepper Motor Driver
* Motore Stepper 28BYJ-48 da 5V DC
* Alimentatore da 6V
* Modulo di alimentazione per Breadboard (opzionale)
* Power Bank da 5V 8000 mAh (opzionale)

## Funzionamento
Quando alimentato, il ventilatore verifica che il suo stato sia "acceso" (stabilito dalla pressione di un pulsante), quindi, misura la temperatura dell'aria, e, se è superiore ad un valore soglia, come prima cosa verifica che sia puntato verso qualcosa o qualcuno non più distante di mezzo metro, usanto il sensore ad ultrasuoni, e dopo aver valutato:

### Se è puntato verso un soggetto
1. Memorizza la distanza rilevata.
2. Raccoglie le letture dei sensori su temperatura e umidità dell'aria, e umidità della pelle del soggetto puntato.
3. Controlla che le letture siano nei loro range accettabili, e fa in modo che non influenzino il calcolo seguente se non lo sono.
4. Effettua una media pesata dei quattro fattori rilevati per ricavare un valore desiderato per la PWM che governa la velocità della ventola.
5. Controlla se l'umidità della pelle si sia abbassata (meno sudore) a intervalli di un minuto. Se non è migliorata, aumenta leggermente il valore della PWM (cumulativo).
6. Il valore usato effettivamente per la velocità della ventola è una media di PWM calcolate in precedenza più l'ultima, per evitare sbalzi bruschi di velocità.
7. Se durante la ripetizione di questo loop non rileva più la presenza di un soggetto puntato, passa al comportamento descritto di seguito.

### Se non è puntato verso un soggetto
1. Effettua una breve rotazione parallela al piano d'appoggio, di pochi gradi in senso orario.
2. Invia un impulso ultrasonico per verificare che stia puntando verso un soggetto in seguito al movimento.
3. Se ha rilevato un soggetto sotto la distanza soglia, torna al comportamento normale descritto sopra per attivare la ventola.
4. Se non ha nuovamente rilevato un soggetto, ripete queste operazioni, invertendo il verso della rotazione e aumentandone via via l'ampiezza per coprire un'area più ampia.

### Inoltre
Se durante la normale gestione della ventola perde il contatto con il soggetto puntato, perché si è mosso o per errore del sensore, passa al suo comportmento "di ricerca" per cercare di riposizionarsi meglio.

## Licenza
Questo progetto è rilasciato sotto licenza GNU General Public License v3.0
