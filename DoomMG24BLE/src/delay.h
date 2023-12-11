/**
 *  DESCRIPTION:
 *  delay utility function.
 *
 */
#ifndef SRC_DELAY_H_
#define SRC_DELAY_H_
#include <stdint.h>
#include "em_timer.h"
#include "main.h"
static inline void delay(uint32_t milliseconds)
{
    milliseconds *= (TICK_TIMER_FREQUENCY_HZ / 1000);    // TICK_TIMER is clocked at 10 MHz
    uint32_t timeNow = TICK_TIMER->CNT;
    while ((uint32_t) TICK_TIMER->CNT - timeNow < milliseconds)
        ;
}
#endif /* SRC_DELAY_H_ */
