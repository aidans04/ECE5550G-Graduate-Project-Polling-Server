#include "polling.h"

static xAPeriodicTask xAperiodicTasks[ MAX_APERIODIC_TASKS ];
static int xAperiodicTaskCount = 0;

void addAperiodicTask( TaskFunction_t pvTaskCode, void *pvParameters, TickType_t xReleaseTime )
{
    if ( xAperiodicTaskCount < MAX_APERIODIC_TASKS )
    {
        xAperiodicTasks[ xAperiodicTaskCount ].pvTaskCode = pvTaskCode;
        xAperiodicTasks[ xAperiodicTaskCount ].pvParameters = pvParameters;
        xAperiodicTasks[ xAperiodicTaskCount ].xReleaseTime = xReleaseTime;
        xAperiodicTaskCount++;
    }
}

void prvRemoveAperiodicTask( xAPeriodicTask pvTask )
{
    for ( int i = 0; i < xAperiodicTaskCount; i++ )
    {
        if ( xAperiodicTasks[ i ] == pvTask )
        {
            // Shift remaining tasks down
            for ( int j = i; j < xAperiodicTaskCount - 1; j++ )
            {
                xAperiodicTasks[ j ] = xAperiodicTasks[ j + 1 ];
            }
            xAperiodicTaskCount--;
            break;
        }
    }
}

void prvPollingServerTask( void *pvParameters )
{
    // POLLING SERVER TASK CODE, checks current time against xReleaseTime of each aperiodic task and executes them if their time has come
    // no loop required since scheduler will call this task periodically, just check for tasks to execute and execute
    TickType_t xCurrentTime = xTaskGetTickCount();
    for ( int i = 0; i < xAperiodicTaskCount; i++ )
    {
        if ( xCurrentTime >= xAperiodicTasks[ i ].xReleaseTime )
        {
            // Execute the aperiodic task
            xAperiodicTasks[ i ].pvTaskCode( xAperiodicTasks[ i ].pvParameters );
            // Remove the task from the list after execution
            prvRemoveAperiodicTask( xAperiodicTasks[ i ] );
            xAperiodicTaskCount--; // Decrement the count after removing the task
        }
    }

}