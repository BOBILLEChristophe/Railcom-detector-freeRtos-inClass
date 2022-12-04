/*

*/

#include "Railcom.h"

// Identifiants des données du canal 1
#define CH1_ADR_LOW  ( 1 << 2 )
#define CH1_ADR_HIGH ( 1 << 3 )

#define QUEUE_1_SIZE 10
#define QUEUE_2_SIZE  3

#define NB_ADDRESS_TO_COMPARE 100                // Nombre de valeurs à comparer pour obtenir l'adresse de la loco
RingBuf<uint16_t, NB_ADDRESS_TO_COMPARE> buffer; // Instance

/* ----- Constructeur   -------------------*/

Railcom::Railcom(uint8_t txPin, uint8_t rxPin) :
  _txPin(txPin),
  _rxPin(rxPin)
{

  xQueue1 = xQueueCreate(QUEUE_1_SIZE, sizeof(uint8_t));
  xQueue2 = xQueueCreate(QUEUE_2_SIZE, sizeof(uint16_t));
  TaskHandle_t railcomParseHandle = NULL;
  TaskHandle_t railcomReceiveHandle = NULL;
  TaskHandle_t railcomSetHandle = NULL;

  Serial1.begin(250000, SERIAL_8N1, _rxPin, _txPin);    // Define and start ESP32 Serial1 port
  
  uint16_t x = 0;
  for (uint8_t i = 0; i < NB_ADDRESS_TO_COMPARE; i++)      // On place des zéros dans le buffer de comparaison
    buffer.push(x);

  xTaskCreatePinnedToCore(this->receiveData, "ReceiveData",  2 * 1024, this, 4, &railcomReceiveHandle, 0); // Création de la tâches pour la réception
  xTaskCreatePinnedToCore(this->parseData,   "ParseData",    2 * 1024, this, 5, &railcomParseHandle,   1); // Création de la tâches pour le traitement
  xTaskCreatePinnedToCore(this->setAddress,  "SetAddress",   1 * 1024, this, 3, &railcomSetHandle,     1); // Création de la tâches pour MAJ adresse
}


/* ----- getAddress   -------------------*/

uint16_t Railcom::gAddress()
{
  return address;
}


/* ----- receiveData   -------------------*/

void Railcom::receiveData(void *p)
{
  TickType_t xLastWakeTime;
  xLastWakeTime = xTaskGetTickCount();

  uint8_t inByte { 0 };
  uint8_t compt{ 0 };
  Railcom *pThis = (Railcom *) p;

for (;;)
  {
    while (Serial1.available() > 0)
    {
      if (compt == 0)
        inByte = '\0';
      else
        inByte = (uint8_t)Serial1.read();
      if (compt < 3)
        xQueueSend(pThis->xQueue1, &inByte, 0);
      compt++;
    }
    compt = 0;
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1)); // toutes les x ms
  }

  
/* ----- parseData   -------------------*/

void Railcom::parseData(void *p)
{
  bool start { false };
  uint16_t temp { 0 };
  byte inByte { 0 };
  uint8_t rxArray[8] { 0 };
  uint8_t rxArrayCnt { 0 };
  byte dccAddr[2] { 0 };
  Railcom *pThis = (Railcom *) p;

  TickType_t xLastWakeTime;
  xLastWakeTime = xTaskGetTickCount();


  byte decodeArray[] = {172, 170, 169, 165, 163, 166, 156, 154, 153, 149, 147, 150, 142, 141, 139, 177, 178, 180, 184, 116,
                        114, 108, 106, 105, 101, 99, 102, 92, 90, 89, 85, 83, 86, 78, 77, 75, 71, 113, 232, 228, 226, 209, 201,
                        197, 216, 212, 210, 202, 198, 204, 120, 23, 27, 29, 30, 46, 54, 58, 39, 43, 45, 53, 57, 51, 15, 240, 225, 31
                       }; // 31 is end of table (0001 1111)

  auto check_4_8_code = [&]() -> bool
  {
    uint8_t index = 0;
    while (inByte != decodeArray[index])
    {
      if (decodeArray[index] == 31)
        return false;
      index++;
    }
    inByte = index;
    return true;
  };


  for (;;)
  {
    do
    {
      xQueueReceive(pThis->xQueue1, &inByte, pdMS_TO_TICKS(portMAX_DELAY));

      if (inByte == '\0')
        start = true;
    } while (!start);
    start = false;

    for (byte i = 0; i < 2; i++)
    {
      if (xQueueReceive(pThis->xQueue1, &inByte, pdMS_TO_TICKS(portMAX_DELAY)) == pdPASS)
      {
        if (inByte >= 0x0F && inByte <= 0xF0)
        {
          if (check_4_8_code())
          {
            rxArray[rxArrayCnt] = inByte;
            rxArrayCnt++;
          }
        }
      }
    }


    if (rxArrayCnt == 2)
    {
      if (rxArray[0] & CH1_ADR_HIGH)
        dccAddr[0] = rxArray[1] | (rxArray[0] << 6);
      if (rxArray[0] & CH1_ADR_LOW)
        dccAddr[1] = rxArray[1] | (rxArray[0] << 6);
      temp = (dccAddr[1] - 128) << 8;
      if (temp < 0)
        temp = dccAddr[0];
      else
        temp += dccAddr[0];

      bool testOk = true;
      uint16_t j = 0;
      buffer.pop(j);
      buffer.push(temp);
      do
      {
        if (buffer[j] != temp)
          testOk = false;
        j++;
      } while (testOk && j <= buffer.size());

      if (testOk)
        xQueueSend(pThis->xQueue2, &temp, 0);
    }
    rxArrayCnt = 0;
    for (byte i = 0; i < 2; i++)
      rxArray[i] = 0;

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10)); // toutes les x ms
  }
}


/* ----- setAddress   -------------------*/

void Railcom::setAddress(void *p)
{
  TickType_t xLastWakeTime;
  xLastWakeTime = xTaskGetTickCount();

  uint16_t address{ 0 };
  Railcom *pThis = (Railcom *) p;

  for (;;)
  {
    address = 0;
    xQueueReceive(pThis->xQueue2, &address, pdMS_TO_TICKS(0));
    pThis->address = address;

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200)); // toutes les x ms
  }
}
  
