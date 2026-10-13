/* SPDX-License-Identifier: MIT
 * Normalize v4l2loopback's timeperframe for PipeWire fraction matching.
 * No frame data is read, no stream is started, and the rate is unchanged.
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static unsigned int gcd(unsigned int a, unsigned int b)
{
    while (b != 0) {
        unsigned int remainder = a % b;
        a = b;
        b = remainder;
    }
    return a;
}

static int normalize(const char *path)
{
    for (unsigned int attempt = 0; attempt < 100; ++attempt) {
        int fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            if (errno == ENOENT || errno == EBUSY) {
                usleep(50000);
                continue;
            }
            perror("open");
            return 1;
        }
        struct v4l2_capability capability = {0};
        if (ioctl(fd, VIDIOC_QUERYCAP, &capability) < 0 ||
            strcmp((const char *)capability.driver, "v4l2 loopback") != 0) {
            fprintf(stderr, "Refusing a non-v4l2loopback device\n");
            close(fd);
            return 1;
        }
        /* exclusive_caps exposes capture only after the relay writer opens.
         * Wait for that, not just for the device pathname to exist.
         */
        unsigned int caps = capability.capabilities & V4L2_CAP_DEVICE_CAPS
            ? capability.device_caps : capability.capabilities;
        if (!(caps & V4L2_CAP_VIDEO_CAPTURE)) {
            close(fd);
            usleep(50000);
            continue;
        }
        struct v4l2_streamparm parameters = {0};
        parameters.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(fd, VIDIOC_G_PARM, &parameters) < 0) {
            perror("VIDIOC_G_PARM");
            close(fd);
            return 1;
        }
        struct v4l2_fract *interval = &parameters.parm.capture.timeperframe;
        if (!interval->numerator || !interval->denominator) {
            fprintf(stderr, "Invalid zero frame interval\n");
            close(fd);
            return 1;
        }
        unsigned int divisor = gcd(interval->numerator, interval->denominator);
        if (divisor > 1) {
            interval->numerator /= divisor;
            interval->denominator /= divisor;
            if (ioctl(fd, VIDIOC_S_PARM, &parameters) < 0) {
                perror("VIDIOC_S_PARM");
                close(fd);
                return 1;
            }
            printf("%s: normalized frame rate %u/%u fps\n", path,
                   interval->denominator, interval->numerator);
            fflush(stdout);
        }
        close(fd);
        return 0;
    }
    fprintf(stderr, "Timed out waiting for the relay writer\n");
    return 1;
}

int main(int argc, char **argv)
{
    int watch = argc == 3 && strcmp(argv[1], "--watch") == 0;
    if (argc != 2 && !watch) {
        fprintf(stderr, "Usage: %s [--watch] /dev/videoN\n", argv[0]);
        return 2;
    }
    const char *path = argv[watch ? 2 : 1];
    do {
        int result = normalize(path);
        if (result != 0)
            return result;
        if (watch)
            sleep(1);
    } while (watch);
    return 0;
}
