#include "scheduler.h"
#include "C:\Users\aidan\OneDrive\Documents\Arduino\libraries\FreeRTOS\src\ECE5550G-Graduate-Project-Polling-Server\polling.h"

/* ── Change to 1 or 2 to select the task set ──────────────────────────── */
#define TASK_SET 2



#define ITERS_PER_MS   525UL
#define ITERS_100MS   (ITERS_PER_MS * 100UL)
#define ITERS_150MS   (ITERS_PER_MS * 150UL)
#define ITERS_200MS   (ITERS_PER_MS * 200UL)
#define ITERS_300MS   (ITERS_PER_MS * 300UL)

TaskHandle_t xHandle1 = NULL;
TaskHandle_t xHandle2 = NULL;
TaskHandle_t xPollingHandle = NULL;
// TaskHandle_t xHandle3 = NULL;
// TaskHandle_t xHandle4 = NULL;

static void burnIter( uint32_t iters )
{
    volatile uint32_t i;
    for( i = 0; i < iters; i++ ) { }
}

static void taskFunc1 ( void *p ) {
  (void)p;
    Serial.print("t1 RUN  ms="); Serial.println(xTaskGetTickCount()*portTICK_PERIOD_MS);
    burnIter(ITERS_100MS);
    Serial.print("t1 DONE ms="); Serial.println(xTaskGetTickCount()*portTICK_PERIOD_MS);
}

static void taskFunc2 ( void *p ) {
  (void)p;
    Serial.print("t2 RUN  ms="); Serial.println(xTaskGetTickCount()*portTICK_PERIOD_MS);
    burnIter(ITERS_200MS);
    Serial.print("t2 DONE ms="); Serial.println(xTaskGetTickCount()*portTICK_PERIOD_MS);
}

static void taskOnceFunc1 ( void *p ) {
  (void)p;
    Serial.print("A1 RUN  ms="); Serial.println(xTaskGetTickCount()*portTICK_PERIOD_MS);
    burnIter(ITERS_200MS);
    Serial.print("A1 DONE ms="); Serial.println(xTaskGetTickCount()*portTICK_PERIOD_MS);
}

static void taskOnceFunc2 ( void *p ) {
  (void)p;
    Serial.print("A2 RUN  ms="); Serial.println(xTaskGetTickCount()*portTICK_PERIOD_MS);
    burnIter(ITERS_100MS);
    Serial.print("A2 DONE ms="); Serial.println(xTaskGetTickCount()*portTICK_PERIOD_MS);
}

static void taskOnceFunc3 ( void *p ) {
  (void)p;
    Serial.print("A3 RUN  ms="); Serial.println(xTaskGetTickCount()*portTICK_PERIOD_MS);
    burnIter(ITERS_200MS);
    Serial.print("A3 DONE ms="); Serial.println(xTaskGetTickCount()*portTICK_PERIOD_MS);
}

extern "C" void vApplicationMallocFailedHook( void )
{
    Serial.println("!!! MALLOC FAILED !!!");
    Serial.flush();
    for(;;);
}

void setup( void )
{
    Serial.begin(9600);
    while( !Serial ) { ; }

    Serial.print("portTICK_PERIOD_MS="); Serial.println(portTICK_PERIOD_MS);
    Serial.println("========================");
    Serial.flush();

    vSchedulerInit();

    vSchedulerPeriodicTaskCreate( taskFunc1, "t1", 512, NULL, 1, &xHandle1,
        pdMS_TO_TICKS(0), pdMS_TO_TICKS(400),  pdMS_TO_TICKS(200), pdMS_TO_TICKS(400)  );
    vSchedulerPeriodicTaskCreate( taskFunc2, "t2", 512, NULL, 2, &xHandle2,
        pdMS_TO_TICKS(0), pdMS_TO_TICKS(600),  pdMS_TO_TICKS(300), pdMS_TO_TICKS(600)  );

    vSchedulerPeriodicTaskCreate( pollingServerTask, "t2", 512, NULL, 2, &xPollingHandle,
        pdMS_TO_TICKS(0), pdMS_TO_TICKS(500),  pdMS_TO_TICKS(300), pdMS_TO_TICKS(500)  );
    addAperiodicTask( taskOnceFunc1, NULL, pdMS_TO_TICKS(200) );
    addAperiodicTask( taskOnceFunc2, NULL, pdMS_TO_TICKS(800) );
    addAperiodicTask( taskOnceFunc3, NULL, pdMS_TO_TICKS(1300) );

    vSchedulerStart();
}

void loop( void ) {}
