/*


      Railcom.h

   © christophe bobille - locoduino.org


*/


#ifndef __RAILCOM_H__
#define __RAILCOM_H__

#include <Arduino.h>
#include <HardwareSerial.h>
#include <RingBuf.h>    // https://github.com/Locoduino/RingBuffer
#include <HardwareSerial.h>

class Railcom
{
  private:
    const gpio_num_t m_rxPin;
    const gpio_num_t m_txPin;
    uint16_t m_address;
    QueueHandle_t xQueue1;
    QueueHandle_t xQueue2;
    HardwareSerial *mySerial;
    static uint8_t m_compt;

    static void IRAM_ATTR receiveData(void *);
    static void IRAM_ATTR parseData(void *);
    static void IRAM_ATTR setAddress(void *);

  public:
    Railcom(const gpio_num_t, const gpio_num_t);
    uint16_t address() const;
};

#endif
