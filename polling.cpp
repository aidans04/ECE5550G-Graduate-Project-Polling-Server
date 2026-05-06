#include "polling.h"

void vPollingServerInit( PollingServer_t *pxServer, TickType_t xPeriod, TickType_t xExecutionTime )
{
    pxServer->xTaskHandle = NULL;
    pxServer->xPeriod = xPeriod;
    pxServer->xExecutionTime = xExecutionTime;
    pxServer->xIsActive = pdFALSE;
}

void vPollingServerStart( PollingServer_t *pxServer )
{
    if ( pxServer->xIsActive == pdFALSE )
    {
        xTaskCreate( prvPollingServerTask, "PollingServer", configMINIMAL_STACK_SIZE, ( void * ) pxServer, tskIDLE_PRIORITY + 1, &pxServer->xTaskHandle );
        pxServer->xIsActive = pdTRUE;
    }
}

void vPollingServerStop( PollingServer_t *pxServer )
{
    if ( pxServer->xIsActive == pdTRUE )
    {
        vTaskDelete( pxServer->xTaskHandle );
        pxServer->xTaskHandle = NULL;
        pxServer->xIsActive = pdFALSE;
    }
}

static void prvPollingServerTask( void *pvParameters )
{
    PollingServer_t *pxServer = ( PollingServer_t * ) pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for ( ; ; )
    {
        vTaskDelayUntil( &xLastWakeTime, pxServer->xPeriod );

        // Simulate work by delaying for the specified execution time
        vTaskDelay( pxServer->xExecutionTime );
    }
}