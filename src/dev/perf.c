#include "device.h"

#define PERF_ADDR 0x1fe95000
#define PERF_SIZE 0x1000

device_t perf_dev = {
    .name = "perf-counter",
    .start = PERF_ADDR,
    .end = PERF_ADDR + PERF_SIZE,
};
