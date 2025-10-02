#ifndef _NXP_SIMTEMP_H
#define _NXP_SIMTEMP_H

#include <linux/types.h>

/* sample structure returned to userspace */
struct simtemp_sample {
    __u64 timestamp_ns;   /* time of measurement */
    __s32 temp_mC;        /* temperature in milliCelsius */
    __u32 flags;          /* status flags */
};

/* operation modes */
enum simtemp_mode {
    MODE_NORMAL = 0,
    MODE_NOISY,
    MODE_RAMP,
};

#endif /* _NXP_SIMTEMP_H */
