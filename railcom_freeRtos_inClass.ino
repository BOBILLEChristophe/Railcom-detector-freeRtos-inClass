
/*
   Programme de lecture, de decodage et d'affichage des messages Railcom ©
   qui retourne l'adresse d'un decodeur (adresse courte ou longue) sur le port serie (115200 bauds)

   Fonctionne exclusivement sur ESP32
   © christophe bobille - locoduino.org

   lib_deps = locoduino/RingBuffer@^1.0.3 / https://github.com/Locoduino/RingBuffer

   Le moniteur série est réglé à 250 Kbs (Serial.begin(250000))
*/

#ifndef ARDUINO_ARCH_ESP32
#error "Select an ESP32 board"
#endif

#include <Arduino.h>
#include "Railcom.h"

#define VERSION "v 1.1"
#define PROJECT "Railcom Detector ESP32 (freeRTOS in class)"
#define AUTHOR  "christophe BOBILLE Locoduino : christophe.bobille@gmail.com"

#define WITH_INTER

#ifdef WITH_INTER

const gpio_num_t interPin0 = GPIO_NUM_18;
const gpio_num_t interPin1 = GPIO_NUM_19;
const gpio_num_t interPin2 = GPIO_NUM_20;
Railcom railcom_0(GPIO_NUM_3, GPIO_NUM_1, GPIO_NUM_18);   // Instance de la classe Railcom
Railcom railcom_1(GPIO_NUM_16, GPIO_NUM_17, GPIO_NUM_19); // Instance de la classe Railcom
Railcom railcom_2(GPIO_NUM_13, GPIO_NUM_14, GPIO_NUM_20); // Instance de la classe Railcom

#else

Railcom railcom_0(GPIO_NUM_3, GPIO_NUM_1);   // Instance de la classe Railcom
Railcom railcom_1(GPIO_NUM_16, GPIO_NUM_17); // Instance de la classe Railcom
Railcom railcom_2(GPIO_NUM_13, GPIO_NUM_14); // Instance de la classe Railcom

#endif

void setup()
{
}


void loop ()
{
  // Affiche toutes les secondes dans le moniteur serie l'adresse de la locomotive
  Serial.printf("Adresse loco 0 = %d\n", railcom_0.address());
  Serial.printf("Adresse loco 1 = %d\n", railcom_1.address());
  Serial.printf("Adresse loco 2 = %d\n", railcom_2.address());
  delay(1000);
}
