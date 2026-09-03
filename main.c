#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define TICKS_GREEN    5U
#define TICKS_YELLOW   2U
#define TICKS_RED      4U
#define QUEUE_BUSY     6U    
#define LOG_LEN       20U

typedef enum { LIGHT_GREEN = 0, LIGHT_YELLOW, LIGHT_RED } LightState_t;

/* status bits */
#define BIT_NIGHT      0U
#define BIT_BUSY       1U
#define BIT_BLINK_ON   2U

#define SET_BIT(reg, n)    ((reg) |=  (uint8_t)(1U << (n)))
#define CLR_BIT(reg, n)    ((reg) &= (uint8_t)~(1U << (n)))
#define TOGGLE_BIT(reg, n) ((reg) ^=  (uint8_t)(1U << (n)))
#define READ_BIT(reg, n)   ((uint8_t)(((reg) >> (n)) & 1U))

static LightState_t light;
static uint8_t      status;        
static uint8_t      ticksLeft;      /* time left in this colour      */
static uint8_t      carsWaiting;
static uint32_t     carsPassed;
static char         logLine[LOG_LEN + 1];  /* +1 for invisible end character */
static uint32_t     totalTicks;

/* Safe input helper */
static int readInt(int *out) {
    char buf[64];
    if (fgets(buf, sizeof(buf), stdin) == NULL) return 0;
    return sscanf(buf, "%d", out) == 1;
}

static uint8_t ticksFor(LightState_t s) {
    if (s == LIGHT_GREEN) {
        if (READ_BIT(status, BIT_BUSY)) {
            return TICKS_GREEN + 2U;
        }
        return TICKS_GREEN;
    } else if (s == LIGHT_YELLOW) {
        return TICKS_YELLOW;
    } else {
        return TICKS_RED;
    }
}

static LightState_t nextState(LightState_t s) {
    if (s == LIGHT_GREEN) return LIGHT_YELLOW;
    if (s == LIGHT_YELLOW) return LIGHT_RED;
    return LIGHT_GREEN;
}

static void resetCrossing(void) {
    light = LIGHT_RED;
    status = 0;
    carsWaiting = 0;
    carsPassed = 0;
    totalTicks = 0;
    ticksLeft = ticksFor(LIGHT_RED);
    
    for (uint8_t i = 0; i < LOG_LEN; i++) {
        logLine[i] = '-';
    }
    logLine[LOG_LEN] = '\0';
}

static void drawLight(void) {
    printf("\n");
    if (READ_BIT(status, BIT_NIGHT)) {
        printf(" ( ) \n");
        if (READ_BIT(status, BIT_BLINK_ON)) {
            printf(" (Y) \n");
        } else {
            printf(" ( ) \n");
        }
        printf(" ( ) \n");
        printf("--- NIGHT MODE ---\n");
    } else {
        if (light == LIGHT_RED)    printf(" (R) \n ( ) \n ( ) \n--- RED ---\n");
        if (light == LIGHT_YELLOW) printf(" ( ) \n (Y) \n ( ) \n--- YELLOW ---\n");
        if (light == LIGHT_GREEN)  printf(" ( ) \n ( ) \n (G) \n--- GREEN ---\n");
    }
    printf("Ticks left: %u | Cars waiting: %u\n", ticksLeft, carsWaiting);
}

static void pushLog(char c) {
    for (uint8_t i = 0; i < LOG_LEN - 1; i++) {
        logLine[i] = logLine[i + 1];
    }
    logLine[LOG_LEN - 1] = c;
}

static void tick(void) {
    totalTicks++;
    if (READ_BIT(status, BIT_NIGHT)) {
        TOGGLE_BIT(status, BIT_BLINK_ON);
        if (READ_BIT(status, BIT_BLINK_ON)) {
            pushLog('y');
        } else {
            pushLog(' ');
        }
        return;
    }

    ticksLeft--;
    if (ticksLeft == 0) {
        light = nextState(light);
        ticksLeft = ticksFor(light);
    }

    if (light == LIGHT_GREEN && carsWaiting > 0) {
        uint8_t passed = (carsWaiting >= 2) ? 2 : carsWaiting;
        carsWaiting -= passed;
        carsPassed += passed;
        
        if (carsWaiting <= QUEUE_BUSY) {
            CLR_BIT(status, BIT_BUSY);
        }
    }

    if (light == LIGHT_GREEN)       pushLog('G');
    else if (light == LIGHT_YELLOW) pushLog('Y');
    else if (light == LIGHT_RED)    pushLog('R');
}

static void addCars(void) {
    int amount = -1;
    printf("How many cars arrived? ");
    if (!readInt(&amount) || amount < 0) {
        printf("Invalid number of cars.\n");
        return;
    }
    
    carsWaiting += (uint8_t)amount;
    if (carsWaiting > QUEUE_BUSY) {
        SET_BIT(status, BIT_BUSY);
    }
    printf("Added %d cars to the queue.\n", amount);
}

static void toggleNight(void) {
    TOGGLE_BIT(status, BIT_NIGHT);
    if (READ_BIT(status, BIT_NIGHT)) {
        SET_BIT(status, BIT_BLINK_ON);
        printf("Night mode ON.\n");
    } else {
        light = LIGHT_RED;
        ticksLeft = ticksFor(LIGHT_RED);
        printf("Day mode ON. Starting at RED.\n");
    }
}

static void showLog(void) {
    printf("\nTimeline (Oldest -> Newest): [%s]\n", logLine);
}

static void printBinary(uint8_t val) {
    for (int i = 7; i >= 0; i--) {
        printf("%d", (val >> i) & 1);
    }
}

static void crossingReport(void) {
    printf("\n--- Crossing Report ---\n");
    printf("Total Ticks  : %lu\n", (unsigned long)totalTicks);
    printf("Cars Passed  : %lu\n", (unsigned long)carsPassed);
    printf("Cars Waiting : %u\n", carsWaiting);
    printf("Night Mode   : %s\n", READ_BIT(status, BIT_NIGHT) ? "YES" : "NO");
    printf("Queue Busy   : %s\n", READ_BIT(status, BIT_BUSY) ? "YES" : "NO");
    printf("Status Byte  : 0b");
    printBinary(status);
    printf(" / 0x%02X\n", status);
}

int main(void) {
    resetCrossing();
    int choice = -1;

    do {
        printf("\n============================\n");
        printf("    TRAFFIC LIGHT ROBOT     \n");
        printf("============================\n");
        printf(" 1) Draw light\n");
        printf(" 2) Step time (1 Tick)\n");
        printf(" 3) Step time (10 Ticks)\n");
        printf(" 4) Add cars\n");
        printf(" 5) Toggle Night/Day Mode\n");
        printf(" 6) Show history log\n");
        printf(" 7) System Report\n");
        printf(" 0) Exit\n");
        printf("Select > ");

        if (!readInt(&choice)) {
            printf("Invalid input.\n");
            continue;
        }

        switch (choice) {
            case 1: drawLight(); break;
            case 2: tick(); printf("1 tick passed.\n"); break;
            case 3: 
                for(int i=0; i<10; i++) tick(); 
                printf("10 ticks passed.\n"); 
                break;
            case 4: addCars(); break;
            case 5: toggleNight(); break;
            case 6: showLog(); break;
            case 7: crossingReport(); break;
            case 0: printf("Shutting down...\n"); break;
            default: printf("Unknown option.\n"); break;
        }
    } while (choice != 0);

    return 0;
}