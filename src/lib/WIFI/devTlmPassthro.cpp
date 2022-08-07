#include "devTlmPassthro.h"

#if defined(USE_MSP_WIFI)
#include "tcpsocket.h"
TCPSOCKET wifi2tcp(5761); //port 5761 as used by BF configurator
#include "CRSF.h"
extern CRSF crsf;
#endif

static bool TlmPassStatus = false;

void HandleTlm2WIFI()
{
  #if defined(USE_MSP_WIFI)
  // check is there is any data to write out
  if (crsf.crsf2msp.FIFOout.peekSize() > 0)
  {
    const uint16_t len = crsf.crsf2msp.FIFOout.popSize();
    uint8_t data[len];
    crsf.crsf2msp.FIFOout.popBytes(data, len);
    wifi2tcp.write(data, len);
  }

  // check if there is any data to read in
  const uint16_t bytesReady = wifi2tcp.bytesReady();
  if (bytesReady > 0)
  {
    uint8_t data[bytesReady];
    wifi2tcp.read(data);
    crsf.msp2crsf.parse(data, bytesReady);
  }

  wifi2tcp.handle();
  #endif
}

void setTlmPassStatus(bool status)
{
  TlmPassStatus = status;
}

bool getTlmPassStatus(void)
{
  return TlmPassStatus;
}