
/*

   Programme de lecture, de decodage et d'affichage des messages Railcom ©
   qui retourne l'adresse d'un decodeur (adresse courte ou longue) sur le port serie (115200 bauds)

   Fonctionne exclusivement sur ESP32
   © christophe bobille - locoduino.org

   lib_deps = locoduino/RingBuffer@^1.0.3 / https://github.com/Locoduino/RingBuffer

*/

#ifndef ARDUINO_ARCH_ESP32
#error "Select an ESP32 board"
#endif

#include <Arduino.h>
#include "Railcom.h"

#define VERSION "v 1.5"
#define PROJECT "Railcom Detector ESP32"
#define AUTHOR  "christophe BOBILLE Locoduino : christophe.bobille@gmail.com"

Railcom railcom_0(GPIO_NUM_3, GPIO_NUM_1);   // Instance de la classe Railcom
Railcom railcom_1(GPIO_NUM_16, GPIO_NUM_17); // Instance de la classe Railcom
Railcom railcom_2(GPIO_NUM_13, GPIO_NUM_14); // Instance de la classe Railcom

void setup()
{
  Serial.begin (250000);

  Serial.printf("\n\nProject :    %s", PROJECT);
  Serial.printf("\nVersion :      %s", VERSION);
  Serial.printf("\nAuthor :       %s", AUTHOR);
  Serial.printf("\nFichier :      %s", __FILE__);
  Serial.printf("\nCompiled  :      %s - %s \n\n", __DATE__, __TIME__);
}


void loop ()
{
  // Affiche toutes les secondes dans le moniteur serie l'adresse de la locomotive
  Serial.printf("Adresse loco 0 = %d\n", railcom_0.address());
  Serial.printf("Adresse loco 1 = %d\n", railcom_1.address());
  Serial.printf("Adresse loco 2 = %d\n", railcom_2.address());

  delay(1000);
}
