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

const byte moistPin = A0; //Il pin di Analog Input che riceve i dati del sensore per l'umidità della pelle
const short minMoist = 200; //Valore ricavato dal sensore di umità della pelle quando è asciutto (da tarare sullo specifico sensore)
const short maxMoist = 800; //Valore ricavato dal sensore di umità della pelle quando è immerso in acqua (da tarare sullo specifico sensore)

const short stepsPerRevolution = 2048;  //Numero di step che compongono una rotazione completa del motore stepper
Stepper myStepper(stepsPerRevolution, 8, 10, 9, 11); //Inizializza l'oggetto di controllo del motore stepper con i GPIO da 8 a 11 per i segnali
//Sul modulo di controllo del motore stepper il pin 8 è connesso a 1N4, 9 a 1N3, 10 a 1N2, e 11 a 1N1

const byte trigPin = 3;  //Output per inviare il segnale di trigger al sensore ad ultrasuoni
const byte echoPin = 2;  //Input per ricevere la distanza rilevata
const short maxDist = 50;  //Massima distanza in cm che considera per essere puntata verso una persona
short distanza = maxDist/2;  //Distanza in cm misurata dal sensore ad ultrasuoni
byte contaDistanzaErrata = 0;  //Contatore per il numero di volte che la distanza è stata rilevata come errata o oltre la maxDist

const byte pwmPin = 5;   //GPIO per trasmettere la PWM al motore della ventola
const byte fanSensoAntiorario = 6;   //Due GPIO per il controllo della direzione di rotazione della ventola
const byte fanSensoOrario = 7;       //Quando uno dei due è HIGH e l'altro è LOW, la ventola gira nel senso indicato dal nome della variabile in HIGH

const short maxTemp = 50;   //temperatura massima misurabile dal sensore (dipende dal sensore, in questo caso DHT11)
//Crea un'istanza della classe DHT11 per gestire il sensore di temperatura e umidità dell'aria
DHT11 dht11(4);   //Definisce il GPIO digitale 4 come input dal sensore


void setup(){
  myStepper.setSpeed(15);   //Imposta la velocità del motore stepper a 15 rpm
  pinMode(moistPin, INPUT);
  pinMode(trigPin, OUTPUT);
	pinMode(echoPin, INPUT);
  pinMode(fanSensoAntiorario, OUTPUT);
  pinMode(fanSensoOrario, OUTPUT);
  Serial.begin(115200);
}


//Main loop
void loop(){
  byte lettureCorrette = 0; //Conta quanti sensori hanno effettuato una lettura corretta, servirà a fare la media pesata per la pwm

  impulso();  //Chiama le istruzioni necessarie per inviare un impulso ultrasonico

  short durata = impulso();
  //Calcolo della distanza dimezzando il tempo passato dall'invio dell'impulso ultrasonico e moltiplicandolo per la velocità del suono in cm/microsecondi
  short dist = (durata*.0343)/2;
  if(dist <= maxDist && dist > 0){  //Controllo della validità della misurazione
    distanza = dist;
    contaDistanzaErrata = 0;
  } else {
    contaDistanzaErrata++;
  }
  //Se ci sono stati troppi rilevamenti errati consecutivi della distanza, la ventola prova a riposizionarsi
  if(contaDistanzaErrata > 3){
    //Funzione per riposizionare la ventola col motore stepper
    distanza = riposizionaVentola();
    contaDistanzaErrata = 0;
  }
  lettureCorrette++;
  byte pwmModDist = map(distanza, 0, maxDist, 100, 255);  //Variabile usata per calcolare la PWM per la ventola, proporzionale alla distanza

  byte pwmModTemp = 0;    //Variabile usata per calcolare la PWM per la ventola, proporzionale alla temperatura
  byte pwmModUmi = 0;     //Variabile usata per calcolare la PWM per la ventola, proporzionale all'umidità dell'aria
  int temperatura, umidita;  //Variabili che saranno usate dal sensore DHT11
  bool tempUmiOK = dht11.readTemperatureHumidity(temperatura, umidita);; //Legge la temperatura e umidità dal sensore DHT11.

  if(tempUmiOK){  //Se la lettura è andata bene, calcola il modificatore per la PWM legato all'umidità dell'aria
    pwmModUmi = map(umidita, 0, 100, 100, 255);
    lettureCorrette++;
  }

  if(tempUmiOK){  //Se la lettura è andata bene, calcola il modificatore per la PWM legato alla temperatura
    pwmModTemp = map(temperatura, 0, maxTemp, 100, 255);
    lettureCorrette++;
  }

  byte pwmModMoist = 0;   //Variabile usata per calcolare la PWM per la ventola, proporzionale all'umidità della pelle
  short valore = analogRead(A0);
  if(valore >= minMoist && valore <= maxMoist){
    pwmModMoist = map(temperatura, minMoist, maxMoist, 100, 255);
    lettureCorrette++;
  }

  byte pwm = 0;   //Nel caso le letture dei sensori siano andate tutte male, la ventola non viene fatta girare

  if(lettureCorrette > 0){  //Effettua la media pesata (pesi da sistemare) dei risultati dei vari sensori per calcolare il valore adeguato per la PWM
    pwm = (pwmModDist + pwmModTemp + pwmModUmi + pwmModMoist)/lettureCorrette;
  }

  digitalWrite(fanSensoAntiorario, LOW);  //Quando è HIGH e l'altra è LOW la ventola gira in senso antiorario
  digitalWrite(fanSensoOrario, HIGH);     //Quando è HIGH e l'altra è LOW la ventola gira in senso orario
  analogWrite(pwmPin, pwm);
  
  delay(1000); //Pausa per un secondo
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
//di una persona. Restituisce la prima distanza letta correttamente.
//Da aggiungere: gestione della totale assenza di una misurazione accettabile
short riposizionaVentola(){
  short i = 1;   //contatore dei movimenti fatti dallo stepper, usato anche per aumentare l'ampiezza della rotazione a ogni iterazione
  short direzione = 1; //usato per stabilire la direzione della rotazione del motore, viene poi invertito per l'iterazione successiva
  short dis;
  short dur;
  while (i<51){
    myStepper.step(i*20*direzione);
    delay(1500);

    short dur = impulso();  //Chiama le istruzioni necessarie per inviare un impulso ultrasonico
    //Calcolo della distanza dimezzando il tempo passato dall'invio dell'impulso ultrasonico e moltiplicandolo per la velocità del suono in cm/microsecondi
    short dis = (dur*.0343)/2;
    if(dis <= maxDist && dis > 0){
      return dis;
    }
    direzione = -direzione;
    i++;
  }
  return -1;  //In futuro usato per indicare l'assenza di misurazione accettabile
}