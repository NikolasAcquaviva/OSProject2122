/**
 * @file helpers.c
 * @author Enrico Emiro
 * @brief Implementazione funzioni helper
 * @date 2022-04-01
 */

#include "utils/helpers.h"

void setPLT(unsigned int time)
{
  int timeScale = *((memaddr *)TIMESCALEADDR);
  setTIMER(time * timeScale);
}

unsigned int isDevice(unsigned int address, unsigned int interruptLine)
{
  return (address >= (memaddr)DEV_REG_ADDR(interruptLine, 0) &&
          address <= (memaddr)DEV_REG_ADDR(interruptLine, 7));
}

unsigned int findDeviceNumber(unsigned int address,
                              unsigned int interruptLine)
{
  int i;
  for (i = 0; i < 8; i++)
    if (address >= DEV_REG_ADDR(interruptLine, i) &&
        address < DEV_REG_ADDR(interruptLine, i + 1))
      break;
  return i;
}

unsigned int getSemaphoreIndex(unsigned int deviceNumber,
                               unsigned int interruptLine,
                               unsigned int readOrWrite)
{
  // E' un terminale
  if (interruptLine == IL_TERMINAL)
    return ((interruptLine - 3) * 8) + (readOrWrite * 8) + deviceNumber;

  // Non è un terminale
  return ((interruptLine - 3) * 8) + deviceNumber;
}
