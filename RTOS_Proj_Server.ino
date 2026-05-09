/*
 * Configuration:
 *   APERIODIC_LOAD 1 or 2 or 3     Light (2000ms) / Medium (800ms) / Heavy (200ms)
 *
 * Serial output tokens (9600 baud):
 *   JOB_ARR  ms=<t>                            aperiodic job posted to queue
 *   JOB_DROP                                   queue was full; job discarded
 *   SERVER_START ms=<t> budget_ms=<b>          polling server activated with budget
 *   SERVER_JOB_EXEC ms=<t> resp=<r>ms          job finished by server; response time
 *   SERVER_DONE ms=<t> used_ms=<u>             server done; budget used
 *   SERVER_IDLE ms=<t>                         server activated but queue empty
 *   STATS n=<n> avg=<a>ms wc=<w>ms             running summary every STATS_EVERY jobs
 *   MISS <name> ...                            periodic deadline miss (from scheduler)
 */

#include "scheduler.h"

// Experiment configuration
#define APERIODIC_LOAD  1   // 1=Light 2000ms  2=Medium 800ms  3=Heavy 200ms
#define STATS_EVERY     20  // Print running summary every N completed jobs

// Polling server configuration
#define SERVER_PERIOD_MS  300UL  // Ts: period (shorter than all periodic tasks so it has highest RMS priority)
#define SERVER_BUDGET_MS  100UL  // Cs: budget per period

// Timing constants for ATmega2560 @ 16 MHz
#define ITERS_PER_MS    525UL
#define JOB_EXEC_MS     50UL
#define JOB_EXEC_ITERS  (ITERS_PER_MS * JOB_EXEC_MS)

#if   (APERIODIC_LOAD == 1)
  #define ARRIVAL_DELAY_MS  2000UL
  #define LOAD_STR          "LIGHT  (2000ms inter-arrival)"
#elif (APERIODIC_LOAD == 2)
  #define ARRIVAL_DELAY_MS  800UL
  #define LOAD_STR          "MEDIUM (800ms  inter-arrival)"
#else
  #define ARRIVAL_DELAY_MS  200UL
  #define LOAD_STR          "HEAVY  (200ms  inter-arrival)"
#endif

// Aperiodic job record
typedef struct {
    TickType_t xArrivalTick;
} AperiodicJob_t;

static QueueHandle_t xAperiodicQueue = NULL;

// Statistics that are updated only by the polling server task (no critical section needed)
static volatile uint16_t usJobsCompleted   = 0;
static volatile uint32_t ulTotalRespMs     = 0;
static volatile uint16_t usWorstRespMs     = 0;

static TaskHandle_t xHandle1 = NULL;
static TaskHandle_t xHandle2 = NULL;
static TaskHandle_t xHandle3 = NULL;
static TaskHandle_t xHandle4 = NULL;
static TaskHandle_t xHandleServer = NULL;

static void burnIter(uint32_t iters)
{
    volatile uint32_t i;
    for (i = 0; i < iters; i++) { }
}

// Periodic tasks: C=100/200/150/300ms, T=400/800/1000/5000ms
static void taskFunc1(void *p) { (void)p; burnIter(ITERS_PER_MS * 100UL); }
static void taskFunc2(void *p) { (void)p; burnIter(ITERS_PER_MS * 200UL); }
static void taskFunc3(void *p) { (void)p; burnIter(ITERS_PER_MS * 150UL); }
static void taskFunc4(void *p) { (void)p; burnIter(ITERS_PER_MS * 300UL); }

// Aperiodic job generator (priority 1, mostly sleeping)
static void aperiodicGeneratorTask(void *p)
{
    (void)p;
    // Brief startup delay so periodic tasks can stabilize first
    vTaskDelay(pdMS_TO_TICKS(500));

    for (;;)
    {
        AperiodicJob_t job;
        job.xArrivalTick = xTaskGetTickCount();

        if (xQueueSend(xAperiodicQueue, &job, 0) == pdPASS)
        {
            Serial.print(F("JOB_ARR ms="));
            Serial.println((uint32_t)job.xArrivalTick * portTICK_PERIOD_MS);
        }
        else
        {
            Serial.println(F("JOB_DROP"));
        }

        vTaskDelay(pdMS_TO_TICKS(ARRIVAL_DELAY_MS));
    }
}

/*
 * Polling Server Task
 *
 * This is called periodically by the scheduler. On each activation:
 * 1. Record the release (budget refresh) tick
 * 2. Calculate budget expiry: release_tick + SERVER_BUDGET_MS
 * 3. While queue non-empty AND current_tick < expiry: dequeue and execute job
 * 4. Log budget usage (no budget carry-over to next period)
 *
 * The scheduler ensures this task is called every SERVER_PERIOD_MS.
 */
static void pollingServerTask(void *p)
{
    (void)p;
    AperiodicJob_t job;

    TickType_t xServerBudgetMs = SERVER_BUDGET_MS;
    TickType_t xServerReleaseTick = xTaskGetTickCount();
    TickType_t xBudgetExpiry = xServerReleaseTick + pdMS_TO_TICKS(xServerBudgetMs);

    Serial.print(F("SERVER_START ms="));
    Serial.print((uint32_t)xServerReleaseTick * portTICK_PERIOD_MS);
    Serial.print(F(" budget_ms="));
    Serial.println((uint32_t)xServerBudgetMs);

    // Dequeue and execute jobs while budget remains
    for (;;)
    {
        // Check if we have time left
        TickType_t xNowTick = xTaskGetTickCount();
        if (xNowTick >= xBudgetExpiry)
        {
            // Budget exhausted
            uint16_t usBudgetUsedMs = (uint16_t)((xNowTick - xServerReleaseTick) * portTICK_PERIOD_MS);
            Serial.print(F("SERVER_DONE ms="));
            Serial.print((uint32_t)xNowTick * portTICK_PERIOD_MS);
            Serial.print(F(" used_ms="));
            Serial.println(usBudgetUsedMs);
            break;
        }

        // Check if there's enough budget remaining to execute a job
        // With server at highest priority (Ts=300ms < all periodic tasks), no preemption occurs.
        TickType_t xTimeRemaining = xBudgetExpiry - xNowTick;
        TickType_t xTimeNeededPerJob = pdMS_TO_TICKS(60); // Estimate
        
        if (xTimeRemaining < xTimeNeededPerJob)
        {
            // Not enough time to reasonably execute another job without budget overrun
            uint16_t usBudgetUsedMs = (uint16_t)((xNowTick - xServerReleaseTick) * portTICK_PERIOD_MS);
            Serial.print(F("SERVER_DONE ms="));
            Serial.print((uint32_t)xNowTick * portTICK_PERIOD_MS);
            Serial.print(F(" used_ms="));
            Serial.println(usBudgetUsedMs);
            break;
        }

        // Try to dequeue a job with 0 timeout (non-blocking)
        if (xQueueReceive(xAperiodicQueue, &job, 0) == pdPASS)
        {
            // Job dequeued; execute it
            burnIter(JOB_EXEC_ITERS);

            TickType_t xJobDoneTick = xTaskGetTickCount();
            uint16_t   usRespMs     = (uint16_t)((xJobDoneTick - job.xArrivalTick)
                                                  * portTICK_PERIOD_MS);

            Serial.print(F("SERVER_JOB_EXEC ms="));
            Serial.print((uint32_t)xJobDoneTick * portTICK_PERIOD_MS);
            Serial.print(F(" resp="));
            Serial.print(usRespMs);
            Serial.println(F("ms"));

            // Update running statistics
            usJobsCompleted++;
            ulTotalRespMs += usRespMs;
            if (usRespMs > usWorstRespMs)
                usWorstRespMs = usRespMs;

            // Print summary every STATS_EVERY completions
            if ((usJobsCompleted % STATS_EVERY) == 0)
            {
                uint16_t usAvgMs = (uint16_t)(ulTotalRespMs / usJobsCompleted);
                Serial.print(F("STATS n="));
                Serial.print(usJobsCompleted);
                Serial.print(F(" avg="));
                Serial.print(usAvgMs);
                Serial.print(F("ms wc="));
                Serial.print(usWorstRespMs);
                Serial.println(F("ms"));
            }
        }
        else
        {
            // Queue empty; idle
            Serial.print(F("SERVER_IDLE ms="));
            Serial.println((uint32_t)xNowTick * portTICK_PERIOD_MS);
            break;
        }
    }
}

// FreeRTOS hooks
extern "C" void vApplicationMallocFailedHook(void)
{
    Serial.println(F("!!! MALLOC FAILED !!!"));
    Serial.flush();
    for (;;);
}

void setup(void)
{
    Serial.begin(9600);
    while (!Serial) { ; }

    Serial.println(F("=== Polling Server Project ==="));
    Serial.println(F("Task Set 1  C=100/200/150/300ms  T=400/800/1000/5000ms"));
    Serial.print(F("Aperiodic Load: ")); Serial.println(F(LOAD_STR));
    Serial.print(F("Job exec demand: ")); Serial.print(JOB_EXEC_MS); Serial.println(F("ms"));
    Serial.print(F("Server period Ts=")); Serial.print(SERVER_PERIOD_MS);
    Serial.print(F("ms, budget Cs=")); Serial.print(SERVER_BUDGET_MS);
    Serial.println(F("ms"));
    Serial.print(F("portTICK_PERIOD_MS=")); Serial.println(portTICK_PERIOD_MS);
    Serial.println(F("======================================================="));
    Serial.flush();

    // Aperiodic job queue with depth 8; each entry is one AperiodicJob_t
    xAperiodicQueue = xQueueCreate(8, sizeof(AperiodicJob_t));
    configASSERT(xAperiodicQueue != NULL);

    // Generator: priority 1 (sleeps most of the time)
    xTaskCreate(aperiodicGeneratorTask, "AGen", 256, NULL, 1, NULL);

    vSchedulerInit();

    // WCET argument is set to 2x actual execution time to absorb tick jitter
    vSchedulerPeriodicTaskCreate(taskFunc1, "t1", 512, NULL, 1, &xHandle1,
        pdMS_TO_TICKS(0),    pdMS_TO_TICKS(400),  pdMS_TO_TICKS(200), pdMS_TO_TICKS(400));
    vSchedulerPeriodicTaskCreate(taskFunc2, "t2", 512, NULL, 2, &xHandle2,
        pdMS_TO_TICKS(0),    pdMS_TO_TICKS(800),  pdMS_TO_TICKS(400), pdMS_TO_TICKS(700));
    vSchedulerPeriodicTaskCreate(taskFunc3, "t3", 512, NULL, 3, &xHandle3,
        pdMS_TO_TICKS(0),    pdMS_TO_TICKS(1000), pdMS_TO_TICKS(300), pdMS_TO_TICKS(1000));
    vSchedulerPeriodicTaskCreate(taskFunc4, "t4", 512, NULL, 4, &xHandle4,
        pdMS_TO_TICKS(0),    pdMS_TO_TICKS(5000), pdMS_TO_TICKS(600), pdMS_TO_TICKS(5000));

    // Polling server task
    vSchedulerPeriodicTaskCreate(pollingServerTask, "PSrv", 512, NULL, 1, &xHandleServer,
        pdMS_TO_TICKS(0),
        pdMS_TO_TICKS(SERVER_PERIOD_MS),
        pdMS_TO_TICKS(150),  // WCET = 2x budget to absorb tick jitter
        pdMS_TO_TICKS(SERVER_PERIOD_MS));

    vSchedulerStart();
}

void loop(void) {}
