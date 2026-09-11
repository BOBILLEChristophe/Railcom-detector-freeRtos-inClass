/*
   Railcom.cpp

   Détecteur RailCom encapsulé en classe C++ pour ESP32.
   Logique de réception et de décodage basée sur
   RailCom Detector ESP32 v4.7.

   © Christophe BOBILLE - Locoduino
*/

#include "Railcom.h"

namespace
{
// -----------------------------------------------------------------------------
// Table de décodage RailCom 4-out-of-8
// raw UART -> valeur 6 bits (0..63)
// 64..66 = mots de contrôle RailCom
// 255 = symbole invalide
// -----------------------------------------------------------------------------
constexpr uint8_t DECODE_ARRAY[256] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,  64,
    255, 255, 255, 255, 255, 255, 255,  51, 255, 255, 255,  52, 255,  53,  54, 255,
    255, 255, 255, 255, 255, 255, 255,  58, 255, 255, 255,  59, 255,  60,  55, 255,
    255, 255, 255,  63, 255,  61,  56, 255, 255,  62,  57, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255,  36, 255, 255, 255,  35, 255,  34,  33, 255,
    255, 255, 255,  31, 255,  30,  32, 255, 255,  29,  28, 255,  27, 255, 255, 255,
    255, 255, 255,  25, 255,  24,  26, 255, 255,  23,  22, 255,  21, 255, 255, 255,
    255,  37,  20, 255,  19, 255, 255, 255,  50, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,  14, 255,  13,  12, 255,
    255, 255, 255,  10, 255,   9,  11, 255, 255,   8,   7, 255,   6, 255, 255, 255,
    255, 255, 255,   4, 255,   3,   5, 255, 255,   2,   1, 255,   0, 255, 255, 255,
    255,  15,  16, 255,  17, 255, 255, 255,  18, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255,  43,  48, 255, 255,  42,  47, 255,  49, 255, 255, 255,
    255,  41,  46, 255,  45, 255, 255, 255,  44, 255, 255, 255, 255, 255, 255, 255,
    255,  66,  40, 255,  39, 255, 255, 255,  38, 255, 255, 255, 255, 255, 255, 255,
     65, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
};

// Contrôles de cohérence de la table 4/8.
// Ces valeurs ont notamment été observées avec la locomotive d'adresse 12.
static_assert(DECODE_ARRAY[0x99] == 8,  "RailCom 4/8 : 0x99 doit decoder en 8");
static_assert(DECODE_ARRAY[0x8E] == 12, "RailCom 4/8 : 0x8E doit decoder en 12");
static_assert(DECODE_ARRAY[0xA3] == 4,  "RailCom 4/8 : 0xA3 doit decoder en 4");
static_assert(DECODE_ARRAY[0xAC] == 0,  "RailCom 4/8 : 0xAC doit decoder en 0");
}

// -----------------------------------------------------------------------------
// Constructeur
// -----------------------------------------------------------------------------
Railcom::Railcom(uart_port_t uartNum, gpio_num_t rxPin)
    : m_uartNum(uartNum),
      m_rxPin(rxPin),
      m_uartQueue(nullptr),
      m_taskHandle(nullptr),
      m_started(false),
      m_address(0),
      m_adr1Data(0),
      m_adr2Data(0),
      m_adr1Valid(false),
      m_adr2Valid(false),
      m_candidateAddress(0),
      m_confirmationCount(0),
      m_lastValidatedAddress(0),
      m_lastValidatedReceptionMs(0),
      m_waitingSecondSymbol(false),
      m_firstDecodedSymbol(0)
{
}

// -----------------------------------------------------------------------------
// API publique
// -----------------------------------------------------------------------------
bool Railcom::begin()
{
    if (m_started)
    {
        return true;
    }

    m_address = 0;
    m_lastValidatedAddress = 0;
    m_lastValidatedReceptionMs = 0;
    resetParserState();

    if (!initUart())
    {
        return false;
    }

    const BaseType_t result = xTaskCreatePinnedToCore(
        Railcom::taskEntry,
        "RailComUART",
        4096,
        this,
        5,
        &m_taskHandle,
        1
    );

    if (result != pdPASS)
    {
        uart_driver_delete(m_uartNum);
        m_uartQueue = nullptr;
        m_taskHandle = nullptr;
        return false;
    }

    m_started = true;
    return true;
}

void Railcom::end()
{
    if (!m_started)
    {
        return;
    }

    if (m_taskHandle != nullptr)
    {
        vTaskDelete(m_taskHandle);
        m_taskHandle = nullptr;
    }

    uart_driver_delete(m_uartNum);
    m_uartQueue = nullptr;

    m_address = 0;
    m_lastValidatedAddress = 0;
    m_lastValidatedReceptionMs = 0;
    resetParserState();

    m_started = false;
}

uint16_t Railcom::address() const
{
    return m_address;
}

bool Railcom::hasAddress() const
{
    return m_address != 0;
}

// -----------------------------------------------------------------------------
// Initialisation UART RailCom
// -----------------------------------------------------------------------------
bool Railcom::initUart()
{
    uart_config_t uartConfig = {};

    uartConfig.baud_rate = RAILCOM_BAUD_RATE;
    uartConfig.data_bits = UART_DATA_8_BITS;
    uartConfig.parity = UART_PARITY_DISABLE;
    uartConfig.stop_bits = UART_STOP_BITS_1;
    uartConfig.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uartConfig.source_clk = UART_SCLK_APB;

    esp_err_t err = uart_param_config(m_uartNum, &uartConfig);
    if (err != ESP_OK)
    {
        return false;
    }

    // RX uniquement : aucun GPIO TX n'est nécessaire.
    err = uart_set_pin(
        m_uartNum,
        UART_PIN_NO_CHANGE,
        m_rxPin,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    );

    if (err != ESP_OK)
    {
        return false;
    }

    err = uart_driver_install(
        m_uartNum,
        RAILCOM_RX_BUFFER_SIZE,
        0,                          // pas de buffer TX
        RAILCOM_EVENT_QUEUE_SIZE,
        &m_uartQueue,
        0
    );

    if (err != ESP_OK)
    {
        m_uartQueue = nullptr;
        return false;
    }

    err = uart_set_rx_full_threshold(
        m_uartNum,
        RAILCOM_RX_FIFO_THRESHOLD
    );

    if (err != ESP_OK)
    {
        uart_driver_delete(m_uartNum);
        m_uartQueue = nullptr;
        return false;
    }

    err = uart_set_rx_timeout(
        m_uartNum,
        RAILCOM_RX_TIMEOUT_SYMBOLS
    );

    if (err != ESP_OK)
    {
        uart_driver_delete(m_uartNum);
        m_uartQueue = nullptr;
        return false;
    }

    uart_flush_input(m_uartNum);

    return true;
}

// -----------------------------------------------------------------------------
// Tâche FreeRTOS
// -----------------------------------------------------------------------------
void Railcom::taskEntry(void *parameter)
{
    Railcom *instance = static_cast<Railcom *>(parameter);

    if (instance != nullptr)
    {
        instance->taskLoop();
    }

    vTaskDelete(nullptr);
}

void Railcom::taskLoop()
{
    uart_event_t event;

    for (;;)
    {
        // Le réveil périodique ne sert qu'au watchdog de perte RailCom.
        // La réception UART elle-même reste entièrement événementielle.
        if (xQueueReceive(
                m_uartQueue,
                &event,
                pdMS_TO_TICKS(50)
            ) == pdTRUE)
        {
            switch (event.type)
            {
                case UART_DATA:
                {
                    size_t remaining = event.size;

                    while (remaining > 0)
                    {
                        uint8_t temp[32];

                        const size_t requested =
                            (remaining < sizeof(temp))
                                ? remaining
                                : sizeof(temp);

                        const int received = uart_read_bytes(
                            m_uartNum,
                            temp,
                            requested,
                            0
                        );

                        if (received <= 0)
                        {
                            break;
                        }

                        for (int i = 0; i < received; ++i)
                        {
                            feedRailComByte(temp[i]);
                        }

                        remaining -= static_cast<size_t>(received);
                    }

                    break;
                }

                case UART_FIFO_OVF:
                case UART_BUFFER_FULL:
                    uart_flush_input(m_uartNum);
                    xQueueReset(m_uartQueue);

                    // On abandonne uniquement la reconstruction en cours.
                    // Une adresse déjà validée reste présente jusqu'au timeout.
                    resetParserState();
                    break;

                case UART_FRAME_ERR:
                case UART_PARITY_ERR:
                case UART_BREAK:
                default:
                    break;
            }
        }

        checkRailComLoss();
    }
}

// -----------------------------------------------------------------------------
// Parseur continu du flux UART RailCom
// -----------------------------------------------------------------------------
//
// Important : un événement UART_DATA n'est PAS une frontière de trame RailCom.
// Le driver peut livrer par exemple :
//
//   [99 8E] [A3 AC]
//
// ou :
//
//   [F0 A3 AC 99] [8E ...]
//
// ou encore couper une paire entre deux événements.
//
// Les octets sont donc traités comme un flux continu.
//
// Un datagramme RailCom utile pour l'adresse contient deux symboles 4/8 :
//
//   symbole 0 : IIII DD
//   symbole 1 : DDDDDD
//
// Pour le canal 1 :
//   ID = 1 -> ADR1 (partie haute)
//   ID = 2 -> ADR2 (partie basse)
// -----------------------------------------------------------------------------
void Railcom::feedRailComByte(uint8_t raw)
{
    const uint8_t decoded = DECODE_ARRAY[raw];

    // Mot invalide ou mot de contrôle RailCom (64..66).
    // Il sert de séparateur naturel et annule une paire incomplète.
    if (decoded > 63)
    {
        m_waitingSecondSymbol = false;
        return;
    }

    if (!m_waitingSecondSymbol)
    {
        const uint8_t identifier = decoded >> 2;

        // Pour la détection d'adresse du canal 1, seuls ID1 et ID2
        // peuvent constituer le premier symbole intéressant.
        if ((identifier == 1) || (identifier == 2))
        {
            m_firstDecodedSymbol = decoded;
            m_waitingSecondSymbol = true;
        }

        return;
    }

    // Un premier symbole ID1/ID2 a déjà été reçu.
    // Tout symbole de données 0..63 est valable en deuxième position.
    processAddressDatagram(m_firstDecodedSymbol, decoded);

    m_waitingSecondSymbol = false;
}

void Railcom::processAddressDatagram(uint8_t symbol0, uint8_t symbol1)
{
    const uint8_t identifier = symbol0 >> 2;

    const uint8_t data = static_cast<uint8_t>(
        ((symbol0 & 0x03) << 6) | symbol1
    );

    switch (identifier)
    {
        case 1: // ADR1 : Address High
            m_adr1Data = data;
            m_adr1Valid = true;
            break;

        case 2: // ADR2 : Address Low
            m_adr2Data = data;
            m_adr2Valid = true;
            break;

        default:
            return;
    }

    if (m_adr1Valid && m_adr2Valid)
    {
        const uint16_t decodedAddress = buildAddress();

        // Les deux morceaux sont consommés ensemble.
        m_adr1Valid = false;
        m_adr2Valid = false;

        validateAddress(decodedAddress);
    }
}

// -----------------------------------------------------------------------------
// Reconstruction de l'adresse DCC
// -----------------------------------------------------------------------------
uint16_t Railcom::buildAddress() const
{
    // Même principe que la V4.7 :
    //
    // ADR1 < 128 :
    //   adresse courte / consist -> adresse dans ADR2
    //
    // ADR1 >= 128 :
    //   adresse étendue -> 6 bits hauts dans ADR1 après retrait du
    //   marqueur 0b10xxxxxx, puis 8 bits bas dans ADR2.
    if (m_adr1Data < 128)
    {
        return m_adr2Data;
    }

    return static_cast<uint16_t>(
        (static_cast<uint16_t>(m_adr1Data - 128) << 8) |
        m_adr2Data
    );
}

// -----------------------------------------------------------------------------
// Validation de l'adresse
// -----------------------------------------------------------------------------
void Railcom::validateAddress(uint16_t address)
{
    if (address == 0)
    {
        m_candidateAddress = 0;
        m_confirmationCount = 0;
        return;
    }

    // Toute adresse différente casse la série de confirmations.
    if (address != m_candidateAddress)
    {
        m_candidateAddress = address;
        m_confirmationCount = 1;
        return;
    }

    if (m_confirmationCount < ADDRESS_CONFIRMATIONS)
    {
        ++m_confirmationCount;
    }

    // Il faut 5 reconstructions identiques consécutives pour :
    // - valider une nouvelle adresse ;
    // - ou confirmer qu'une adresse déjà validée est toujours présente.
    if (m_confirmationCount < ADDRESS_CONFIRMATIONS)
    {
        return;
    }

    const uint32_t now = millis();

    if (address != m_lastValidatedAddress)
    {
        m_lastValidatedAddress = address;
        m_address = address;
    }

    // Le watchdog de présence n'est rafraîchi qu'après un groupe complet
    // de 5 confirmations consécutives.
    m_lastValidatedReceptionMs = now;

    // On repart de zéro pour exiger un nouveau groupe de 5 confirmations
    // avant le prochain rafraîchissement du watchdog.
    m_confirmationCount = 0;
}

// -----------------------------------------------------------------------------
// Surveillance de la perte RailCom
// -----------------------------------------------------------------------------
void Railcom::checkRailComLoss()
{
    if (m_lastValidatedAddress == 0)
    {
        return;
    }

    const uint32_t now = millis();

    if (static_cast<uint32_t>(
            now - m_lastValidatedReceptionMs
        ) < RAILCOM_LOSS_TIMEOUT_MS)
    {
        return;
    }

    // Aucun nouveau groupe valide depuis 1 seconde :
    // la locomotive est considérée absente.
    m_address = 0;
    m_lastValidatedAddress = 0;
    m_lastValidatedReceptionMs = 0;

    resetParserState();
}

// -----------------------------------------------------------------------------
// Réinitialisation du parseur
// -----------------------------------------------------------------------------
void Railcom::resetParserState()
{
    m_adr1Data = 0;
    m_adr2Data = 0;
    m_adr1Valid = false;
    m_adr2Valid = false;

    m_candidateAddress = 0;
    m_confirmationCount = 0;

    m_waitingSecondSymbol = false;
    m_firstDecodedSymbol = 0;
}
