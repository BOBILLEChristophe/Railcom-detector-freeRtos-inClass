
/*
   Programme de lecture, de décodage et d'affichage des messages Railcom ©
   qui retourne l'adresse d'un décodeur (adresse courte ou longue) sur le port serie (115200 bauds)

   Fonctionne exclusivement sur ESP32
   © christophe bobille

   lib_deps = locoduino/RingBuffer@^1.0.3 / https://github.com/Locoduino/RingBuffer

*/

#ifndef ARDUINO_ARCH_ESP32
#error "Select an ESP32 board"
#endif

#include <Arduino.h>

#define VERSION "v 1.0"
#define PROJECT "Railcom Detector ESP32 (freeRTOS in class)"

#include "Railcom.h"

#define RAILCOM_RX                    GPIO_NUM_14
//#define RAILCOM_RX                    GPIO_NUM_33
#define RAILCOM_TX                    GPIO_NUM_17

Railcom railcom(RAILCOM_TX, RAILCOM_RX);


/*——————————————————————————————————————————————————————————————————————————————
   SETUP
  ————————————————————————————————————————————————————————————————————————————--*/

void setup() {
  //--- Start serial
  Serial.begin (115200) ;
  delay (1000) ;

  Serial.printf("\n\nProject :    %s", PROJECT);
  Serial.printf("\nVersion :      %s", VERSION);
  Serial.printf("\nFichier :      %s", __FILE__);
  Serial.printf("\nCompiled :     %s", __DATE__);
  Serial.printf(" - %s\n\n", __TIME__);

  for (;;)
  {
    Serial.printf("Adresse loco = %d\n", railcom.gAddress());
    delay(1000);
  }
}


void loop ()
{}
