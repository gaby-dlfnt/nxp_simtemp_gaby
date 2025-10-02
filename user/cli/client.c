#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include "../../kernel/nxp_simtemp.h"   // struct simtemp_sample

#define SYSFS_BASE   "/sys/class/misc/simtemp"

static volatile int keep_running = 1;

static void handle_sigint(int sig)
{
    (void)sig;
    keep_running = 0;
}

static int read_sysfs_attr(const char *attr, char *buf, size_t buflen)
{
    char path[256];
    int fd, n;

    snprintf(path, sizeof(path), "%s/%s", SYSFS_BASE, attr);

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open sysfs");
        return -1;
    }

    n = read(fd, buf, buflen - 1);
    if (n < 0) {
        perror("read sysfs");
        close(fd);
        return -1;
    }
    buf[n] = '\0';
    close(fd);
    return 0;
}

static int read_sampling_ms(void)
{
    char buf[64];
    int val = 100; /* default */

    if (read_sysfs_attr("sampling_ms", buf, sizeof(buf)) == 0) {
        val = atoi(buf);
    }
    return val;
}

int main(void)
{
    const char *dev = "/dev/simtemp"; 
    int fd = open(dev, O_RDONLY);
    if (fd < 0) {
        perror("open /dev/simtemp");
        return 1;
    }

    signal(SIGINT, handle_sigint);  /* allow Ctrl-C to stop */

    printf("Press Ctrl-C to stop...\n\n");

    while (keep_running) {
        struct simtemp_sample sample;
        ssize_t n = read(fd, &sample, sizeof(sample));
        if (n < 0) {
            perror("read sample");
            break;
        }

        if (n != sizeof(sample)) {
            fprintf(stderr, "Short read: got %zd bytes\n", n);
            break;
        }

        printf("=== Sample ===\n");
        printf("Timestamp: %lu ns\n", sample.timestamp_ns);
        printf("Temperature: %.3f °C\n", sample.temp_mC / 1000.0);
        printf("Flags: 0x%x\n", sample.flags);

        /* also show sysfs info */
        char buf[128];

        if (read_sysfs_attr("mode", buf, sizeof(buf)) == 0)
            printf("Mode: %s", buf);

        if (read_sysfs_attr("stats", buf, sizeof(buf)) == 0)
            printf("Stats: %s", buf);

        if (read_sysfs_attr("sampling_ms", buf, sizeof(buf)) == 0)
            printf("Sampling_ms: %s", buf);

        if (read_sysfs_attr("threshold_mC", buf, sizeof(buf)) == 0)
            printf("Threshold_mC: %s\n", buf);

        /* wait according to sampling_ms */
        int period_ms = read_sampling_ms();
        usleep(period_ms * 1000);
    }

    close(fd);
    printf("\nExiting...\n");
    return 0;
}
