/*
   Railcom.h

   Détecteur RailCom encapsulé en classe C++ pour ESP32.
   Réception UART événementielle via le driver ESP-IDF.

   Logique de décodage issue de RailCom Detector ESP32 v4.7 :
   - UART matériel 250000 bauds, 8N1
   - réception événementielle
   - parseur continu indépendant des frontières UART_DATA
   - décodage RailCom 4-out-of-8, canal 1
   - validation après 5 adresses identiques consécutives
   - perte de l'adresse après 1 seconde sans nouvelle validation

   © Christophe BOBILLE - Locoduino
*/

#ifndef RAILCOM_H
#define RAILCOM_H

#ifndef ARDUINO_ARCH_ESP32
#error "Select an ESP32 board"
#endif

#include <Arduino.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

class Railcom
{
public:
    Railcom(uart_port_t uartNum, gpio_num_t rxPin);

    // Initialise l'UART et démarre la tâche FreeRTOS RailCom.
    // Retourne true si l'initialisation a réussi.
    bool begin();

    // Arrête la tâche et libère le driver UART.
    void end();

    // Adresse DCC RailCom actuellement validée.
    // 0 signifie qu'aucune locomotive n'est actuellement validée.
    uint16_t address() const;

    bool hasAddress() const;

private:
    // -------------------------------------------------------------------------
    // Configuration RailCom / UART
    // -------------------------------------------------------------------------
    static constexpr uint32_t RAILCOM_BAUD_RATE = 250000;

    // Le FIFO matériel de l'ESP32 classique fait 128 octets.
    // Le buffer driver doit être strictement supérieur au FIFO.
    static constexpr int RAILCOM_RX_BUFFER_SIZE = 256;
    static constexpr int RAILCOM_EVENT_QUEUE_SIZE = 20;

    // Une rafale RailCom normale est normalement livrée par timeout,
    // et non parce que le FIFO est plein.
    static constexpr uint8_t RAILCOM_RX_FIFO_THRESHOLD = 120;

    // À 250 kbit/s en 8N1 : 1 symbole UART ~= 40 us.
    // 2 symboles donnent donc un timeout d'environ 80 us.
    static constexpr uint8_t RAILCOM_RX_TIMEOUT_SYMBOLS = 2;

    // Nombre de reconstructions identiques consécutives avant validation.
    static constexpr uint8_t ADDRESS_CONFIRMATIONS = 5;

    // Perte RailCom après 1 seconde sans nouveau groupe valide.
    static constexpr uint32_t RAILCOM_LOSS_TIMEOUT_MS = 1000;

    // -------------------------------------------------------------------------
    // Configuration de l'instance
    // -------------------------------------------------------------------------
    const uart_port_t m_uartNum;
    const gpio_num_t m_rxPin;

    QueueHandle_t m_uartQueue;
    TaskHandle_t m_taskHandle;
    bool m_started;

    // -------------------------------------------------------------------------
    // État RailCom propre à chaque instance
    // -------------------------------------------------------------------------
    volatile uint16_t m_address;

    uint8_t m_adr1Data;
    uint8_t m_adr2Data;
    bool m_adr1Valid;
    bool m_adr2Valid;

    uint16_t m_candidateAddress;
    uint8_t m_confirmationCount;

    uint16_t m_lastValidatedAddress;
    uint32_t m_lastValidatedReceptionMs;

    bool m_waitingSecondSymbol;
    uint8_t m_firstDecodedSymbol;

    // -------------------------------------------------------------------------
    // Gestion UART / FreeRTOS
    // -------------------------------------------------------------------------
    bool initUart();

    static void taskEntry(void *parameter);
    void taskLoop();

    // -------------------------------------------------------------------------
    // Décodage RailCom
    // -------------------------------------------------------------------------
    void feedRailComByte(uint8_t raw);
    void processAddressDatagram(uint8_t symbol0, uint8_t symbol1);

    uint16_t buildAddress() const;
    void validateAddress(uint16_t address);
    void checkRailComLoss();

    void resetParserState();
};

#endif
