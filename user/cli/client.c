// simtemp_client.c
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "../../kernel/nxp_simtemp_ioctl.h"   // same struct

int main(void) {
    const char *dev = "/dev/simtemp";  // adjust to your device node
    int fd = open(dev, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    struct simtemp_sample sample;
    ssize_t n = read(fd, &sample, sizeof(sample));
    if (n < 0) {
        perror("read");
        close(fd);
        return 1;
    }

    if (n != sizeof(sample)) {
        fprintf(stderr, "Short read: got %zd bytes\n", n);
        close(fd);
        return 1;
    }

    printf("Timestamp: %lu ns\n", sample.timestamp_ns);
    printf("Temperature: %.3f °C\n", sample.temp_mC / 1000.0);
    printf("Flags: 0x%x\n", sample.flags);

    close(fd);
    return 0;
}
