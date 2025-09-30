#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "../../kernel/nxp_simtemp_ioctl.h"   // same struct

#define SYSFS_BASE   "/sys/class/misc/simtemp"

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

int main(void) {
    const char *dev = "/dev/simtemp"; 
    int fd = open(dev, O_RDONLY);
    if (fd < 0) {
        perror("open /dev/simtemp");
        return 1;
    }

    struct simtemp_sample sample;
    ssize_t n = read(fd, &sample, sizeof(sample));
    if (n < 0) {
        perror("read sample");
        close(fd);
        return 1;
    }

    if (n != sizeof(sample)) {
        fprintf(stderr, "Short read: got %zd bytes\n", n);
        close(fd);
        return 1;
    }

    printf("=== Sample from /dev/simtemp ===\n");
    printf("Timestamp: %lu ns\n", sample.timestamp_ns);
    printf("Temperature: %.3f °C\n", sample.temp_mC / 1000.0);
    printf("Flags: 0x%x\n\n", sample.flags);

    close(fd);

    char buf[128];

    if (read_sysfs_attr("sampling_ms", buf, sizeof(buf)) == 0)
        printf("sampling_ms: %s", buf);

    if (read_sysfs_attr("threshold_mC", buf, sizeof(buf)) == 0)
        printf("threshold_mC: %s", buf);

    if (read_sysfs_attr("stats", buf, sizeof(buf)) == 0)
        printf("stats: %s", buf);

    return 0;
}
