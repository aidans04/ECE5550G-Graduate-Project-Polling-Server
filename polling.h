#ifndef POLLING_H_
#define POLLING_H_

#include <Arduino_FreeRTOS.h>
#include <Arduino.h>

#define MAX_APERIODIC_TASKS 5

struct xAPeriodicTask {
    TaskFunction_t pvTaskCode;
    //const char *pcName;
    void *pvParameters;
    TickType_t xReleaseTime;
};

void addAperiodicTask( TaskFunction_t pvTaskCode, void *pvParameters, TickType_t xReleaseTime );
void prvRemoveAperiodicTask( int index );
void pollingServerTask( void *pvParameters );

#endif /* POLLING_H_ */
