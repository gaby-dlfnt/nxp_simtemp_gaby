#ifndef _NXP_SIMTEMP_IOCTL_H
#define _NXP_SIMTEMP_IOCTL_H

#include <stdint.h> 

struct simtemp_sample {
    uint64_t timestamp_ns;  // monotonic timestamp
    int32_t  temp_mC;       // milli-degree Celsius
    uint32_t flags;         // bits for events
} __attribute__((packed));

#endif