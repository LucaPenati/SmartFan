/*
Programma di controllo per una "Smart Fan", un piccolo ventilatore che reagisce alle condizioni ambientali, controllato da una board Arduino UNO
Copyright (C) 2026  Luca Penati

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <DHT11.h> //Libreria per l'omonimo lettore di temperatura e umidità di Dhruba Saha su licenza MIT https://github.com/dhrubasaha08/DHT11/blob/main/
#include <Stepper.h>  //Libreria per il controllo del motore stepper

#define RESET_PIN A2    //GPIO utilizzato per inviare il segnale di reset per spegnere il ventilatore
#define BUTTON_PIN 2    //GPIO collegato al pulsante di accensione/spegnimento del ventilatore 
#define DEBOUNCE_DELAY 50   //Intervallo di attesa per il debounce del pulsante
volatile unsigned long ultimaPressione = 0; //Ultima volta che il pulsante è stato premuto, da usare nella funzione ISR chiamata dall'interrupt alla pressione del pulsante
bool acceso = false;    //Variabile che contiene lo stato acceso/spento del ventilatore in base alla pressione del pulsante

short pwm = 0;   //Variabile che memorizza l'ultimo valore usato per controllare la velocità della ventola tramite PWM
#define LUNG_STORICO 5  //Lunghezza dell'array seguente
int storicoErrori[LUNG_STORICO] = {0, 0, 0, 0, 0};  //Memorizza gli ultimi 5 "errori", la distanza tra il valore di PWM voluto e quello effettivo
byte index = 0;         //Indice che sarà usato per scrivere nell'array sopra
bool primiAccessi = true;  //Usato per stabilire quando lo storicoErrori sta venendo riempito per la prima volta, in modo da ignorare gli zeri di dichiarazione (differenziandoli da eventuali 0 dovuti allo stato "a regime")

#define MOIST_PIN A0    //Il pin di Analog Input che riceve i dati del sensore per l'umidità della pelle
#define MIN_MOIST 490   //Valore ricavato dal sensore di umità della pelle quando è asciutto (da tarare sullo specifico sensore)
#define MAX_MOIST 190   //Valore ricavato dal sensore di umità della pelle quando è immerso in acqua (da tarare sullo specifico sensore)
//Braccio bagnato: tra 236 e 270
//Braccio asciutto: attorno a 400-410

#define STEPS_REVOLUTION 2048   //Numero di step che compongono una rotazione completa del motore stepper
#define STEPS_MOVEMENT 20       //Numero di step per ogni movimento del motore (usato nella funzione "riposizionaVentola")
Stepper myStepper(STEPS_REVOLUTION, 8, 10, 9, 11); //Inizializza l'oggetto di controllo del motore stepper con i GPIO da 8 a 11 per i segnali
//Sul modulo di controllo del motore stepper il pin 8 è connesso a 1N4, 9 a 1N3, 10 a 1N2, e 11 a 1N1

#define TRIG_PIN 4   //Output per inviare il segnale di trigger al sensore ad ultrasuoni
#define ECHO_PIN 3   //Input per ricevere il tempo passato tra l'invio dell'impulso ultrasonico e la ricezione della sua eco
#define MAX_DIST 50  //Massima distanza in cm che considera per essere puntata verso una persona
short distanza = MAX_DIST/2;  //Distanza in cm misurata dal sensore ad ultrasuoni. E' inizializzata in tal modo per il caso in cui il ventilatore non stia puntando un soggetto all'accensione
byte contaDistanzaErrata = 0; //Contatore per il numero di volte che la distanza è stata rilevata come errata o oltre la maxDist

#define PWM_PIN 5           //GPIO per trasmettere il valore usato per la PWM al ponte h che controlla il motore della ventola
#define FAN_ANTIORARIO 6    //Due GPIO per il controllo della direzione di rotazione della ventola
#define FAN_ORARIO 7        //Quando uno dei due è HIGH e l'altro è LOW, la ventola gira nel senso indicato dal nome della variabile in HIGH

#define MIN_TEMP 20       //Temperatura minima alla quale il ventilatore avvia la ventola (arbitraria)
#define FINESTRA_TEMP 2   //Margine al di sotto della MIN_TEMP che mantiene la ventola in funzione, se la temperatura cala fino a o sotto MIN_TEMP - FINESTRA_TEMP, arresta la ventola
#define MAX_TEMP 40       //Temperatura massima considerata per stabilire proporzionalmente la velocità della ventola
bool accesoPerTemperatura = false;  /*Questa variabile tiene presente quando il ventilatore è da considerarsi acceso per raggiungimento o superamento della MIN_TEMP, 
                                      va considerata assieme alla variabile "acceso" precedententemente dichiarata */

//Crea un'istanza della classe DHT11 per gestire il sensore di temperatura e umidità dell'aria
DHT11 dht11(12);   //Definisce il GPIO digitale 12 come input dal sensore


void setup(){
  digitalWrite(RESET_PIN, HIGH);   //Il GPIO viene configurato per essere sempre HIGH in OUTPUT, perché viene usato nella ISR alla pressione
  pinMode(RESET_PIN, OUTPUT);      //del bottone per inviare un segnale LOW al pin di RESET quando lo scopo è spegnere il ventilatore.
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  //La pressione del pulsante attiva un interrupt, in questo modo in qualsiasi momento il pulsante venga premuto, il controllore risponde
  //agendo di conseguenza.
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), gestioneBottone, FALLING);
  pinMode(MOIST_PIN, INPUT);
  myStepper.setSpeed(10);   //Imposta la velocità del motore stepper a 10 rpm
  pinMode(TRIG_PIN, OUTPUT);
	pinMode(ECHO_PIN, INPUT);
  pinMode(FAN_ANTIORARIO, OUTPUT);
  pinMode(FAN_ORARIO, OUTPUT);
}


//Main loop
void loop(){
  //Le funzioni del ventilatore saranno eseguite solo se lo stato del sistema è "acceso", e la temperatura è stata letta correttamente
  //ed è superiore alla soglia minima. Se il sensore non è in grado di leggere la temperatura, viene considerato solo se l'utente
  //abbia acceso o meno il ventilatore.
  if(acceso){
    
    int temperatura = 0;  //Variabili che saranno usate dal sensore DHT11
    int umidita = 0;
    int tempUmiFail = dht11.readTemperatureHumidity(temperatura, umidita); //Legge la temperatura e umidità dal sensore DHT11.
    //Nota 1: restituisce 0 se la lettura è andata bene, che corrisponde normalmente ad un false booleano, da cui il nome della variabile.
    //Nota 2: ho utilizzato la funzione che legge entrambe le grandezze assieme perché in questo sistema se rischieste separatamente
    //viene sollevato un errore di timeout per la seconda misura richiesta (es. umidità se richiesta dopo temperatura)

    if((tempUmiFail == 0 && temperatura >= MIN_TEMP) || tempUmiFail != 0){
      accesoPerTemperatura = true;  //Se la temperatura è superiore alla soglia (oppure se il sensore è rotto), la ventola si avvia
      /*Nota: accesoPerTemperatura essendo variabile globale rimane a "true" anche qualora la temperatura cada sotto la soglia minima
        questo per evitare repentini avvii e arresti nell'intorno della MIN_TEMP. */
    }

    //Se la temperatura è caduta oltre la soglia di arresto, il ventilatore si ferma (pur rimanendo tecnicamente "acceso" tramite variabile associata alla pressione del pulsante)
    if(tempUmiFail == 0 && temperatura <= (MIN_TEMP - FINESTRA_TEMP)){
      accesoPerTemperatura = false;
      pwm = 0;
    }

    if(accesoPerTemperatura){
      //Pesi per il calcolo tramite media pesata del valore target per il controllo della velocità della ventola via PWM
      byte pesoDist = 0;
      byte pesoTemp = 0;
      byte pesoUmi = 0;
      byte pesoMoist = 0;
    
      byte lettureCorrette = 0; //Conta quanti sensori hanno effettuato una lettura corretta, servirà a fare la media pesata per il valore PWM target
      byte pwmModTemp = 0;    //Variabile usata per calcolare la PWM voluta per la ventola, proporzionale alla temperatura
      byte pwmModUmi = 0;     //Variabile usata per calcolare la PWM voluta per la ventola, proporzionale all'umidità dell'aria

      if(tempUmiFail == 0){  //Se la lettura è andata bene, calcola il modificatore per la PWM legato all'umidità dell'aria
        pwmModUmi = map(umidita, 0, 100, 130, 255);
        pesoUmi = 3;
        lettureCorrette++;
      }

      if(tempUmiFail == 0){  //Se la lettura è andata bene, calcola il modificatore per la PWM legato alla temperatura
        if(temperatura < MAX_TEMP){
          pwmModTemp = map(temperatura, (MIN_TEMP - FINESTRA_TEMP), MAX_TEMP, 130, 255);
        } else {  //Se la temperatua è maggiore o uguale alla temperatura di soglia massima, il modificatore PWM è massimizzato
          pwmModTemp = 255;
        }
        pesoTemp = 3;
        lettureCorrette++;
      }

      //Chiama le istruzioni necessarie per inviare un impulso ultrasonico
      short durata = impulso();
      //Calcolo della distanza dimezzando il tempo passato dall'invio dell'impulso ultrasonico e moltiplicandolo per la velocità del suono in cm/microsecondi
      short dist = (durata*.0343)/2;
      if(dist <= MAX_DIST && dist > 0){  //Controllo della validità della misurazione
        distanza = dist;
        contaDistanzaErrata = 0;
      } else {
        contaDistanzaErrata++;
      }
      pesoDist = 4;
      byte pwmModDist = 0;  //Variabile usata per calcolare la PWM voluta per la ventola, proporzionale alla distanza

      //Se ci sono stati troppi rilevamenti errati consecutivi della distanza, la ventola prova a riposizionarsi
      if(contaDistanzaErrata > 3){
        //Funzione per riposizionare la ventola col motore stepper
        distanza = riposizionaVentola();
        if(distanza < 0) {    //Gestione del caso non sia riuscito a individuare un soggetto da puntare
          pwmModDist = 0;     //Non considera la distanza per il calcolo della PWM. Questo permette al ventilatore di funzionare anche se il sensore ad ultrasuoni sia rotto.
          pesoDist = 0;
        } else {
          lettureCorrette++;
          pwmModDist = map(distanza, 0, MAX_DIST, 130, 255);
        }
        contaDistanzaErrata = 0;  //Resetta il contatore per non ricadere immediatamente nel loop di riposizionamento se la prossima lettura del sensore è una distanza non valida
      } else {
        lettureCorrette++;
        pwmModDist = map(distanza, 0, MAX_DIST, 130, 255);
      }

      byte pwmModMoist = 0;   //Variabile usata per calcolare la PWM voluta per la ventola, proporzionale all'umidità della pelle
      short valore = analogRead(MOIST_PIN);
      if(valore <= MIN_MOIST && valore >= MAX_MOIST){   //Nel caso del sensore utilizzato, MIN_MOIST è il limite superiore, e MAX_MOIST quello inferiore
        pwmModMoist = map(valore, MIN_MOIST, MAX_MOIST, 130, 255);  //Più "valore" è un numero basso (tendente a MAX_MOIST), più verrà convertito ad un numero tendente a 255
        pesoMoist = 6;
        lettureCorrette++;
      }

      //Banale e semplicistica gestione dei pesi in caso di malfunzionamento dei sensori, considerata in base ai pesi stabiliti e cosa succede quando i vari sensori falliscono
      /*
        I pesi normalmente sono:
        - pesoDist = 4;
        - pesoTemp = 3;
        - pesoUmi = 3;
        - pesoMoist = 6;
        Inoltre, per come sono raccolte, le variabili di temperatura e peso, se falliscono, falliscono assieme. Entrambi i loro pesi saranno quindi a 0 se il loro sensore malfunziona.
      */
      if((pesoDist + pesoTemp + pesoUmi + pesoMoist) > (lettureCorrette * 4)){  //Letture corrette è moltiplicato per 4 perché in realtà tutti i pesi andrebbero divisi per 4, come sono nella media ponderata sotto
        /*La somma dei pesi risulta maggiore di lettureCorrette*4 solo se pesoMoist è attivo (pari a 6), quindi o sono fallite le letture di temperatura e umidità, o sono fallite sia esse che quelle della distanza.
          Infatti, visti i pesi di temperatura e umidità pari a 3 ciascuno, se fallisce solo la lettura della distanza la somma delle tre variabili rimaste è 12, pari a lettureCorrette*4 (3*4), quindi non
          serve modificare i pesi. */
        if(pesoDist>0){
          pesoDist--;     //Caso in cui solo il sensore per temperatura e umidità abbia fallito, quindi bisogna ribilanciare
          pesoMoist--;    //i pesi rimanenti per la distanza e umidità della pelle in modo che la loro somma sia pari a 8
        } else {
          pesoMoist = pesoMoist - 2;  //Caso in cui la lettura dell'umidità della pelle sia l'unica valida
        }
      } else if ((pesoDist + pesoTemp + pesoUmi + pesoMoist) < (lettureCorrette*4)){
        /*Questo caso avviene solo se la lettura dell'umidità della pelle sia fallita, visto il suo peso di 6. I casi da distinguere sono dunque se solo quel sensore abbia fallito, o sia fallita anche la lettura
          della distanza. Se fossero falliti sia il sensore per la pelle che quello per temperatura e umidità, non servirebbero cambiamenti perché il peso della distanza è 4, pari a lettureCorrette*4 (1*4). */
        if(pesoDist>0){
          pesoDist++;     //Caso in cui solo il sensore di umidità della pelle ha fallito. Viene aumentato anche il peso della temperatura perché
          pesoTemp++;     //nelle prove con la ventola reale, dare troppo peso alla distanza aumenta rapidamente il valore usato per la PWM rispetto a quando tutto funziona
        } else {
          pesoTemp++;     //Caso in cui sia il sensore di umidità della pelle che quello della distanza abbiano fallito, lasciando solo temperatura e umidità a guidare la ventola
          pesoUmi++;      //Essendo di base entrambi i pesi a 3, con questo vanno a 4, raggiungendo il bilanciamento con lettureCorrette*4, evitando di causare problemi al calcolo della media
        }
      }

      short newPwm = 0;    //Variabile che conterrà il valore target per guidare la velocità della ventola tramite PWM

      if(lettureCorrette > 0){  //Effettua la media ponderata dei risultati dei vari sensori per calcolare il valore adeguato per la PWM voluta
        newPwm = ((pesoDist*pwmModDist)/4 +
                (pesoTemp*pwmModTemp)/4 +
                (pesoUmi*pwmModUmi)/4 +
                (pesoMoist*pwmModMoist)/4) / lettureCorrette;
      }
      //Effettua un controllo qualora i calcoli abbiano prodotto valori non validi, e restituisce un valore adeguato a seconda del caso
      newPwm = checkPWM(newPwm);

      //Calcolo della nuova variazione della PWM tramite controllo PID per smorzare cambiamenti repentini della velocità (la newPwm calcolata sopra tende ad oscillare tra le iterazioni)
      int P = newPwm - pwm;   //Errore proporzionale, inteso come differenza tra il valore voluto per la PWM e il valore attuale
      int I = 0;              //Errore integrativo, visto come media degli errori "P" precedentemente inseriti nello storicoErrori
      int D = newPwm - pwm;   /*Errore derivativo, la velocità di avvicinamento/allontanamento dall'obiettivo, che nella situazione discreta a tempo costante
                                (situazione che si ha a meno che non si attivi riposizionaVentola, che considero come un'eccezione ignorabile),
                                calcolo allo stesso modo di P  */

      storicoErrori[index] = P; //Inserisce il nuovo errore proporzionale nello storico

      byte conta = LUNG_STORICO;
      if(primiAccessi){      //Se l'array dello storicoErrori non è ancora stato riempito, il numero di elementi da calcolare è pari a index + 1;
        conta = index + 1;
      }

      if(conta > 0) {
        for (byte i = 0; i < conta; i++){
          I = I + storicoErrori[i];
        }
        I = I / conta;
      }

      //Aggiornamento della prossima cella da scrivere nello storicoErrori all'iterazione successiva del main loop
      if(index >= (LUNG_STORICO - 1)){
        index = 0;
        primiAccessi = false;
      } else {
        index++;
      }

      //Calcolo del nuovo valore per la PWM creato basandosi sugli errori
      pwm = pwm + (4*P/10) + (2*I/10) + (4*D/10);

      //Effettua un controllo qualora i calcoli abbiano prodotto valori non validi, e restituisce un valore adeguato a seconda del caso
      pwm = checkPWM(pwm);
    }
  }

  digitalWrite(FAN_ANTIORARIO, LOW);  //Quando è HIGH e l'altra è LOW la ventola gira in senso antiorario
  digitalWrite(FAN_ORARIO, HIGH);     //Quando è HIGH e l'altra è LOW la ventola gira in senso orario
  analogWrite(PWM_PIN, pwm);

  delay(1000); //Pausa per un secondo, con un ritardo minore la lettura dei sensori ad ogni loop risulta erratica
}


//Funzione chiamata dall'Interrupt legato al pulsante di accensione/spegnimento.
//Se il sistema è spento, lo setta ad acceso, se è già acceso, invece, resetta il sistema, che reinizializza anche la variabile "acceso" a false.
void gestioneBottone(){
  unsigned long timestamp = millis();
  if(timestamp - ultimaPressione > DEBOUNCE_DELAY){
    acceso = !acceso;
    if(!acceso){
      digitalWrite(RESET_PIN, LOW);
    }
  }
  ultimaPressione = timestamp;
}

//Controlla che un valore ottenuto per la PWM sia valido (tra 0 e 255).
//Restituisce lo stesso valore ricevuto in ingresso se è nel range accettabile, altrimenti 0 se il valore era negativo, oppure 255 se era oltre tale soglia.
short checkPWM(short valorePWM){
  if(valorePWM > 255){
    return 255;
  } else if (valorePWM < 0){
    return 0;
  } else {
    return valorePWM;
  }
}


//Questa funzione triggera l'invio di un impulso ultrasonico con l'apposito sensore.
//Restituisce il tempo in microsecondi passato tra l'invio dell'impulso e la ricezione dell'eco.
short impulso(){
  digitalWrite(TRIG_PIN, LOW);   //Imposta il trigger pin a LOW per 2 microsecondi per assicurarsi un segnale pulito
	delayMicroseconds(2);
	digitalWrite(TRIG_PIN, HIGH);  //Imposta il trigger pin a HIGH per 10 microsecondi
	delayMicroseconds(10);
	digitalWrite(TRIG_PIN, LOW);   //Riporta il trigger pin a LOW
  
  return pulseIn(ECHO_PIN, HIGH);
}


//Questa funzione usa il motore stepper per riposizionare la ventola finché il sensore ad ultrasuoni non restituisce
//un valore accettabile, ovvero non negativo ed inferiore alla distanza soglia che viene considerata per la presenza
//di una persona.
//Restituisce la prima distanza letta correttamente, o un valore negativo se non è riuscito a registrare una distanza valida entro i limiti di movimento
short riposizionaVentola(){
  short i = 1;   //contatore dei movimenti fatti dallo stepper, usato anche per aumentare l'ampiezza della rotazione a ogni iterazione
  short direzione = 1; //usato per stabilire la direzione della rotazione del motore, viene poi invertito per l'iterazione successiva
  short dis;  //Distanza in cm
  short dur;  //Durata in microsecondi
  while (i <= (STEPS_REVOLUTION / (2 * STEPS_MOVEMENT))){   //Approssimativamente permette di fare uno scan di 180°
    myStepper.step(i * STEPS_MOVEMENT * direzione);
    delay(2000);

    dur = impulso();  //Chiama le istruzioni necessarie per inviare un impulso ultrasonico
    //Calcolo della distanza dimezzando il tempo passato dall'invio dell'impulso ultrasonico e moltiplicandolo per la velocità del suono in cm/microsecondi
    dis = (dur*.0343)/2;
    if(dis <= MAX_DIST && dis > 0){
      return dis;
    }
    direzione = -direzione;
    i++;
  }
  //Le seguenti istruzioni riportano la ventola nella posizione iniziale
  i = i/2;
  myStepper.step(i * STEPS_MOVEMENT * direzione);
  delay(2000);
  return -1;
}
