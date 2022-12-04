/*


*/

#ifndef __RAILCOM_H__
#define __RAILCOM_H__

#include <Arduino.h>
#include "Config.h"
#include <RingBuf.h>

class Railcom
{
  private:
    uint8_t _rxPin;
    uint8_t _txPin;
    uint16_t address{ 0 };
    QueueHandle_t xQueue1;
    QueueHandle_t xQueue2;

    const byte decodeArray[68] = {172, 170, 169, 165, 163, 166, 156, 154, 153, 149, 147, 150, 142, 141, 139, 177, 178, 180, 184, 116,
                                  114, 108, 106, 105, 101, 99, 102, 92, 90, 89, 85, 83, 86, 78, 77, 75, 71, 113, 232, 228, 226, 209, 201,
                                  197, 216, 212, 210, 202, 198, 204, 120, 23, 27, 29, 30, 46, 54, 58, 39, 43, 45, 53, 57, 51, 15, 240, 225, 31
                                 }; // 31 is end of table (0001 1111)

    static void receiveData(void *);
    static void parseData(void *);
    static void setAddress(void *);

  public:
    Railcom(uint8_t, uint8_t);
    uint16_t gAddress();
};

#endif
