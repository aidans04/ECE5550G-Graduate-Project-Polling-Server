#ifndef POLLING_H_
#define POLLING_H_

#include <Arduino_FreeRTOS.h>

typedef struct PollingServer
{
    TaskHandle_t xTaskHandle;
    TickType_t xPeriod;
    TickType_t xExecutionTime;
    BaseType_t xIsActive;
} PollingServer_t;

void vPollingServerInit( PollingServer_t *pxServer, TickType_t xPeriod, TickType_t xExecutionTime );
void vPollingServerStart( PollingServer_t *pxServer );
void vPollingServerStop( PollingServer_t *pxServer );
void prvPollingServerTask( void *pvParameters );

#endif /* POLLING_H_ */
