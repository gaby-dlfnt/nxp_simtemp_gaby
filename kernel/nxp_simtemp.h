#ifndef _NXP_SIMTEMP_H
#define _NXP_SIMTEMP_H

    struct simtemp_sample {
        __u64 timestamp_ns;   // monotonic timestamp
        __s32 temp_mC;        // milli-degree Celsius
        __u32 flags;          // bits for events
    } __attribute__((packed));




#endif /* _NXP_SIMTEMP_H */
