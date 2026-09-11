/*
   railcom_freeRtos_inClass_v2.ino

   Programme de test de la classe Railcom.

   Basé sur la logique RailCom Detector ESP32 v4.7 :
   - UART ESP-IDF événementielle
   - 250000 bauds, 8N1
   - décodage RailCom 4-out-of-8 canal 1
   - adresses DCC courtes et longues
   - 5 confirmations consécutives
   - disparition après 1 seconde sans nouvelle validation

   Fonctionne exclusivement sur ESP32.

   © Christophe BOBILLE - Locoduino
*/

#ifndef ARDUINO_ARCH_ESP32
#error "Select an ESP32 board"
#endif

#include <Arduino.h>
#include "Railcom.h"

#define VERSION "v 2.0"
#define PROJECT "RailCom Detector ESP32 - inClass"
#define AUTHOR "Christophe BOBILLE - Locoduino"

// -----------------------------------------------------------------------------
// Configuration des détecteurs RailCom
// -----------------------------------------------------------------------------
//
// L'ESP32 classique possède 3 UART matériels : UART0, UART1 et UART2.
// Chaque détecteur RailCom utilise son propre UART.
//
// Avec UART0 réservé à Serial : 2 détecteurs RailCom maximum.
// Sans Serial sur UART0         : 3 détecteurs RailCom maximum.
// -----------------------------------------------------------------------------

// Détecteur RailCom n°1
constexpr uart_port_t RAILCOM_UART_1 = UART_NUM_1;
constexpr gpio_num_t RAILCOM_RX_PIN_1 = GPIO_NUM_4;

Railcom railcom1(RAILCOM_UART_1, RAILCOM_RX_PIN_1);


// Détecteur RailCom n°2
constexpr uart_port_t RAILCOM_UART_2 = UART_NUM_2;
constexpr gpio_num_t RAILCOM_RX_PIN_2 = GPIO_NUM_16;

Railcom railcom2(RAILCOM_UART_2, RAILCOM_RX_PIN_2);


// -----------------------------------------------------------------------------
// Détecteur RailCom n°3
// -----------------------------------------------------------------------------
//
// ATTENTION : le troisième détecteur utilise UART0.
//
// UART0 ne peut pas être utilisé simultanément par RailCom et par Serial.
// Si USE_RAILCOM_3 vaut true, le moniteur série classique ne doit donc pas
// être initialisé avec Serial.begin().
// -----------------------------------------------------------------------------

#define USE_RAILCOM_3 false
#if USE_RAILCOM_3

constexpr uart_port_t RAILCOM_UART_3 = UART_NUM_0;
constexpr gpio_num_t RAILCOM_RX_PIN_3 = GPIO_NUM_3;

Railcom railcom3(RAILCOM_UART_3, RAILCOM_RX_PIN_3);

#endif

// -----------------------------------------------------------------------------
void setup() {

#if !USE_RAILCOM_3
  // UART0 reste disponible pour le moniteur série
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("==========================================");
  Serial.printf("Projet  : %s\n", PROJECT);
  Serial.printf("Version : %s\n", VERSION);
  Serial.printf("Auteur  : %s\n", AUTHOR);
  Serial.printf("Fichier : %s\n", __FILE__);
  Serial.printf("Compile : %s - %s\n", __DATE__, __TIME__);
  Serial.println("==========================================");

  Serial.printf(
    "RailCom 1 : UART%d - RX GPIO%d - 250000 bauds\n",
    static_cast<int>(RAILCOM_UART_1),
    static_cast<int>(RAILCOM_RX_PIN_1));

  Serial.printf(
    "RailCom 2 : UART%d - RX GPIO%d - 250000 bauds\n\n",
    static_cast<int>(RAILCOM_UART_2),
    static_cast<int>(RAILCOM_RX_PIN_2));
#endif


  // ---------------------------------------------------------------------------
  // Démarrage des détecteurs RailCom 1 et 2
  // ---------------------------------------------------------------------------

  if (!railcom1.begin()) {

#if !USE_RAILCOM_3
    Serial.println("ERREUR : impossible d'initialiser RailCom 1.");
#endif

    while (true) {
      delay(1000);
    }
  }


  if (!railcom2.begin()) {

#if !USE_RAILCOM_3
    Serial.println("ERREUR : impossible d'initialiser RailCom 2.");
#endif

    while (true) {
      delay(1000);
    }
  }


  // ---------------------------------------------------------------------------
  // Troisième détecteur optionnel
  // ---------------------------------------------------------------------------

#if USE_RAILCOM_3

  if (!railcom3.begin()) {

    // Pas de Serial ici puisque UART0 est utilisé par RailCom 3.
    while (true) {
      delay(1000);
    }
  }

#else

  Serial.println("Détecteurs RailCom démarrés.");
  Serial.println("En attente des locomotives...");

#endif
}


// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
void loop() {

#if !USE_RAILCOM_3

  // ---------------------------------------------------------------------------
  // Avec 2 détecteurs RailCom :
  // UART0 reste disponible pour le moniteur série.
  // ---------------------------------------------------------------------------

  static uint16_t previousAddress1 = 0xFFFF;
  static uint16_t previousAddress2 = 0xFFFF;

  const uint16_t currentAddress1 = railcom1.address();
  const uint16_t currentAddress2 = railcom2.address();


  // ---------------------------------------------------------------------------
  // Détecteur RailCom n°1
  // ---------------------------------------------------------------------------

  if (currentAddress1 != previousAddress1) {

    if (currentAddress1 == 0) {
      Serial.println("RailCom 1 : aucune locomotive détectée.");
    } else {
      Serial.printf(
        "RailCom 1 : adresse locomotive validée : %u\n",
        currentAddress1);
    }

    previousAddress1 = currentAddress1;
  }


  // ---------------------------------------------------------------------------
  // Détecteur RailCom n°2
  // ---------------------------------------------------------------------------

  if (currentAddress2 != previousAddress2) {

    if (currentAddress2 == 0) {
      Serial.println("RailCom 2 : aucune locomotive détectée.");
    } else {
      Serial.printf(
        "RailCom 2 : adresse locomotive validée : %u\n",
        currentAddress2);
    }

    previousAddress2 = currentAddress2;
  }


#else

  // ---------------------------------------------------------------------------
  // Avec 3 détecteurs RailCom :
  // UART0 est utilisé par RailCom 3.
  // Aucun accès à Serial n'est donc possible.
  //
  // Les trois adresses restent bien entendu accessibles par le programme.
  // ---------------------------------------------------------------------------

  const uint16_t currentAddress1 = railcom1.address();
  const uint16_t currentAddress2 = railcom2.address();
  const uint16_t currentAddress3 = railcom3.address();

  // Ici, l'application peut exploiter directement :
  //
  // currentAddress1
  // currentAddress2
  // currentAddress3
  //
  // Par exemple pour les transmettre sur le bus CAN,
  // les utiliser dans les satellites autonomes, etc.

#endif

  delay(10);
}
