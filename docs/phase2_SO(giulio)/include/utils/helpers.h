/**
 * @file helpers.h
 * @author Enrico Emiro
 * @author Giulio Billi
 * @brief Header funzioni helper
 * @date 2022-04-01
 */
#ifndef HELPERS_H
#define HELPERS_H

/**
 * @brief Setta il PLT in base al valore presente nel registro TimeScale
 * (0x1000.0024)
 * @param time Tempo da settare
 */
void setPLT(unsigned int time);

/**
 * @brief
 *
 * @param address
 * @param interruptLine
 * @return int
 */
unsigned int isDevice(unsigned int address, unsigned int interruptLine);

/**
 * @brief
 *
 */
unsigned int findDeviceNumber(unsigned int address, unsigned int interruptLine);

/**
 * @brief
 *
 * @param deviceNumber
 * @param interruptLine
 * @param readOrWrite
 */
unsigned int getSemaphoreIndex(unsigned int deviceNumber,
                               unsigned int interruptLine,
                               unsigned int readOrWrite);

#endif
