/*
Programma di controllo per una "Smart Fan", un piccolo ventilatore che reagisce alle condizioni ambientali
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

#include <DHT11.h> //Libreria per il lettore di temperatura e umidità di Dhruba Saha su licenza MIT https://github.com/dhrubasaha08/DHT11/blob/main/
#include <Stepper.h>  //Libreria per il controllo del motore stepper

//Questa funzione fa ripartire l'esecuzione dall'istruzione 0, essenzialmente resettando il programma al suo stato iniziale
void(* reset)(void) = 0;

const byte buttonPin = 2;  //GPIO collegato al pulsante di accensione/spegnimento del ventilatore
bool acceso = false;        //Variabile che contiene lo stato acceso/spento del ventilatore

const byte moistPin = A0;   //Il pin di Analog Input che riceve i dati del sensore per l'umidità della pelle
const short minMoist = 490; //Valore ricavato dal sensore di umità della pelle quando è asciutto (da tarare sullo specifico sensore)
const short maxMoist = 190; //Valore ricavato dal sensore di umità della pelle quando è immerso in acqua (da tarare sullo specifico sensore)
//Braccio bagnato: tra 236 e 270
//Braccio asciutto: attorno a 400-410

const short stepsPerRevolution = 2048;  //Numero di step che compongono una rotazione completa del motore stepper
Stepper myStepper(stepsPerRevolution, 8, 10, 9, 11); //Inizializza l'oggetto di controllo del motore stepper con i GPIO da 8 a 11 per i segnali
//Sul modulo di controllo del motore stepper il pin 8 è connesso a 1N4, 9 a 1N3, 10 a 1N2, e 11 a 1N1

const byte trigPin = 4;  //Output per inviare il segnale di trigger al sensore ad ultrasuoni
const byte echoPin = 3;  //Input per ricevere la distanza rilevata
const short maxDist = 50;  //Massima distanza in cm che considera per essere puntata verso una persona
short distanza = maxDist/2;  //Distanza in cm misurata dal sensore ad ultrasuoni. E' inizializzata in tal modo per il caso in cui il ventilatore non stia puntando un soggetto all'accensione
byte contaDistanzaErrata = 0;  //Contatore per il numero di volte che la distanza è stata rilevata come errata o oltre la maxDist

const byte pwmPin = 5;   //GPIO per trasmettere la PWM al motore della ventola
const byte fanSensoAntiorario = 6;  //Due GPIO per il controllo della direzione di rotazione della ventola
const byte fanSensoOrario = 7;      //Quando uno dei due è HIGH e l'altro è LOW, la ventola gira nel senso indicato dal nome della variabile in HIGH

const short minTemp = 20;   //Temperatura minima sopra la quale il ventilatore si accende (arbitraria)
const short maxTemp = 40;   //Temperatura massima considerata per stabire proporzionalmente la velocità della ventola
//Crea un'istanza della classe DHT11 per gestire il sensore di temperatura e umidità dell'aria
DHT11 dht11(12);   //Definisce il GPIO digitale 4 come input dal sensore


void setup(){
  pinMode(buttonPin, INPUT_PULLUP);
  //La pressione del pulsante attiva un interrupt, in questo modo in qualsiasi momento il pulsante venga premuto, il controllore risponde
  //agendo di conseguenza.
  attachInterrupt(digitalPinToInterrupt(buttonPin), gestioneBottone, FALLING);
  pinMode(moistPin, INPUT);
  myStepper.setSpeed(10);   //Imposta la velocità del motore stepper a 10 rpm
  pinMode(trigPin, OUTPUT);
	pinMode(echoPin, INPUT);
  pinMode(fanSensoAntiorario, OUTPUT);
  pinMode(fanSensoOrario, OUTPUT);
}


//Main loop
void loop(){
  byte pwm = 0; //Valore per il controllo della velocità della ventola tramite PWM

  //Le funzioni del ventilatore saranno eseguite solo se lo stato del sistema è "acceso", e la temperatura è stata letta correttamente
  //ed è superiore alla soglia minima. Se il sensore non è in grado di leggere la temperatura, viene considerato solo se l'utente
  //abbia acceso o meno il ventilatore.
  if(acceso){
    
    int temperatura, umidita;  //Variabili che saranno usate dal sensore DHT11
    bool tempUmiOK = dht11.readTemperatureHumidity(temperatura, umidita);; //Legge la temperatura e umidità dal sensore DHT11.

    if((tempUmiOK && temperatura >= minTemp) || !tempUmiOK){
      //Pesi per il calcolo tramite media pesata del valore usato per controllare la velocità della ventola via PWM
      byte pesoDist = 0;
      byte pesoTemp = 0;
      byte pesoUmi = 0;
      byte pesoMoist = 0;
    
      byte lettureCorrette = 0; //Conta quanti sensori hanno effettuato una lettura corretta, servirà a fare la media pesata per la pwm
      byte pwmModTemp = 0;    //Variabile usata per calcolare la PWM per la ventola, proporzionale alla temperatura
      byte pwmModUmi = 0;     //Variabile usata per calcolare la PWM per la ventola, proporzionale all'umidità dell'aria

      if(tempUmiOK){  //Se la lettura è andata bene, calcola il modificatore per la PWM legato alla temperatura
        if(temperatura < maxTemp){
          pwmModTemp = map(temperatura, 0, maxTemp, 130, 255);
        } else {  //Se la temperatua è maggiore o uguale alla temperatura di soglia massima, il modificatore PWM è massimizzato
          pwmModTemp = 255;
        }
        pesoTemp = 3;
        lettureCorrette++;
      }

      if(tempUmiOK){  //Se la lettura è andata bene, calcola il modificatore per la PWM legato alla temperatura
        pwmModTemp = map(temperatura, 0, maxTemp, 130, 255);
        pesoTemp = 3;
        lettureCorrette++;
      }

      //Chiama le istruzioni necessarie per inviare un impulso ultrasonico
      short durata = impulso();
      //Calcolo della distanza dimezzando il tempo passato dall'invio dell'impulso ultrasonico e moltiplicandolo per la velocità del suono in cm/microsecondi
      short dist = (durata*.0343)/2;
      if(dist <= maxDist && dist > 0){  //Controllo della validità della misurazione
        distanza = dist;
        contaDistanzaErrata = 0;
      } else {
        contaDistanzaErrata++;
      }

      byte pwmModDist = 0;  //Variabile usata per calcolare la PWM per la ventola, proporzionale alla distanza

      //Se ci sono stati troppi rilevamenti errati consecutivi della distanza, la ventola prova a riposizionarsi
      if(contaDistanzaErrata > 3){
        //Funzione per riposizionare la ventola col motore stepper
        distanza = riposizionaVentola();
        if(distanza < 0) {    //Gestione del caso non sia riuscito a individuare un soggetto da puntare
          pwmModDist = 0;     //Non considera la distanza per il calcolo della PWM. Questo permette al ventilatore di funzionare anche se il sensore ad ultrasuoni sia rotto.
        }
        contaDistanzaErrata = 0;
      } else {
        lettureCorrette++;
        pesoDist = 4;
        pwmModDist = map(distanza, 0, maxDist, 130, 255);
      }

      byte pwmModMoist = 0;   //Variabile usata per calcolare la PWM per la ventola, proporzionale all'umidità della pelle
      short valore = analogRead(A0);
      if(valore >= minMoist && valore <= maxMoist){
        pwmModMoist = map(valore, minMoist, maxMoist, 130, 255);
        pesoMoist = 6;
        lettureCorrette++;
      }

      //Banale e semplicistica gestione dei pesi in caso di malfunzionamento dei sensori
      if((pesoDist + pesoTemp + pesoUmi + pesoMoist) > (lettureCorrette*4)){
        if(pesoDist>0){
          pesoDist--;
          pesoMoist--;
        } else {
          pesoMoist = pesoMoist - 2;
        }
      } else if ((pesoDist + pesoTemp + pesoUmi + pesoMoist) < (lettureCorrette*4)){
        if(pesoDist>0){
          pesoDist = pesoDist + 2;
        } else {
          pesoTemp++;
          pesoUmi++;
        }
      }

      if(lettureCorrette > 0){  //Effettua la media pesata (pesi da sistemare) dei risultati dei vari sensori per calcolare il valore adeguato per la PWM
        pwm = ((pesoDist*pwmModDist)/4 +
              (pesoTemp*pwmModTemp)/4 +
              (pesoUmi*pwmModUmi)/4 +
              (pesoMoist*pwmModMoist)/4) / lettureCorrette;
      }

      //Nel caso il calcolo sopra sfori la soglia massima per un qualsiasi motivo imprevisto
      if(pwm > 255){
        pwm = 255;
      }
    }
  }
  digitalWrite(fanSensoAntiorario, LOW);  //Quando è HIGH e l'altra è LOW la ventola gira in senso antiorario
  digitalWrite(fanSensoOrario, HIGH);     //Quando è HIGH e l'altra è LOW la ventola gira in senso orario
  analogWrite(pwmPin, pwm);

  delay(50); //Pausa per 50 millisecondi
}


//Funzione chiamata dall'Interrupt legato al bottone
//Se il sistema è spento, lo setta ad acceso, se è già acceso, invece, resetta il sistema, che reinizializza anche la variabile "acceso" a false.
void gestioneBottone(){
  acceso = !acceso;
  if(!acceso){
    reset();
  }
}


//Questa funzione triggera l'invio di un impulso ultrasonico con l'apposito sensore.
//Restituisce la distanza letta in cm
short impulso(){
  digitalWrite(trigPin, LOW);   //Imposta il trigger pin a LOW per 2 microsecondi per assicurarsi un segnale pulito
	delayMicroseconds(2);
	digitalWrite(trigPin, HIGH);  //Imposta il trigger pin a HIGH per 10 microsecondi
	delayMicroseconds(10);
	digitalWrite(trigPin, LOW);   //Riporta il trigger pin a LOW
  
  return pulseIn(echoPin, HIGH);
}


//Questa funzione usa il motore stepper per riposizionare la ventola finché il sensore ad ultrasuoni non restituisce
//un valore accettabile, ovvero non negativo ed inferiore alla distanza soglia che viene considerata per la presenza
//di una persona.
//Restituisce la prima distanza letta correttamente, o un valore negativo se non è riuscito a registrare una distanza valida
short riposizionaVentola(){
  short i = 1;   //contatore dei movimenti fatti dallo stepper, usato anche per aumentare l'ampiezza della rotazione a ogni iterazione
  short direzione = 1; //usato per stabilire la direzione della rotazione del motore, viene poi invertito per l'iterazione successiva
  short dis;
  short dur;
  while (i<51){   //Approssimativamente permette di fare uno scan di 180°
    myStepper.step(i*20*direzione);
    delay(2000);

    short dur = impulso();  //Chiama le istruzioni necessarie per inviare un impulso ultrasonico
    //Calcolo della distanza dimezzando il tempo passato dall'invio dell'impulso ultrasonico e moltiplicandolo per la velocità del suono in cm/microsecondi
    short dis = (dur*.0343)/2;
    if(dis <= maxDist && dis > 0){
      return dis;
    }
    direzione = -direzione;
    i++;
  }
  //Le seguenti istruzioni riportano la ventola nella posizione iniziale
  i = i/2;
  myStepper.step(i*20*direzione);
  delay(2000);
  return -1;
}
