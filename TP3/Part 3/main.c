// TP3 - part 3
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define ADXL_IOCTL_SET_AXIS_X _IO('X', 0)
#define ADXL_IOCTL_SET_AXIS_Y _IO('Y', 1)
#define ADXL_IOCTL_SET_AXIS_Z _IO('Z', 2)

#define DEVICE_PATH "/dev/adxl345-0"

// int main(int argc, char *argv[]) {
int main() {
    int fd = open(DEVICE_PATH, O_RDWR); // Read and write
    if (fd == -1) {
        perror("Failed to open the device");
        return EXIT_FAILURE;
    }
    char axis;
    int num_sample;
    printf("Which axis do you want to read data from? [X, Y, Z]: ");
    scanf("%c", &axis);
    printf("How many bytes do you want to read from %c-axis?: ", axis);
    scanf("%d", &num_sample);


    switch (axis) {
        case 'X':
            ioctl(fd, ADXL_IOCTL_SET_AXIS_X, 0);
            break;
        case 'Y':
            ioctl(fd, ADXL_IOCTL_SET_AXIS_Y, 1);
            break;
        case 'Z':
            ioctl(fd, ADXL_IOCTL_SET_AXIS_Z, 2);
            break;
        default:
            fprintf(stderr, "Invalid axis. Use X, Y, or Z\n");
            close(fd);
            return EXIT_FAILURE;
    }

    // Read data from the accelerometer
    short sample;
    ssize_t ret = read(fd, &sample, num_sample);

    if (ret == -1) {
        perror("Error reading from device file");
        close(fd);
        return EXIT_FAILURE;
    }

    if(num_sample > 2) {
        printf("[Warning] Maximum number of bytes from X-axis is 2 !!\n");
        num_sample = 2;
    }
    printf("%d byte(s) of sample read from %c-axis of the accelerometer is: %04X\n", num_sample, axis, sample);

    close(fd);
    return 0;
}