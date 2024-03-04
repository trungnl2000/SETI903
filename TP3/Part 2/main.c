//TP3 part2
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#define DEVICE_PATH "/dev/adxl345-0"  // Path to device file

int main() {
    int fd;
    short sample;
    // Open the device file
    fd = open(DEVICE_PATH, O_RDONLY); // Read only
    if (fd == -1) {
        perror("Failed to open the device file");
        return EXIT_FAILURE;
    }

    // Read data from the device file
    int num_sample;
    printf("How many bytes do you want to read from X-axis?: ");
    scanf("%d", &num_sample);
    ssize_t rd = read(fd, &sample, num_sample);
    if (rd < 0) {
        perror("Failed to read data from the device file");
        close(fd);
        return EXIT_FAILURE;
    }

    // Display the read data
    if(num_sample > 2) {
        printf("[Warning] Maximum number of bytes from X-axis is 2 !!\n");
        num_sample = 2;
    }
    printf("%d byte(s) of sample read from X-axis of the accelerometer is: %04X\n", num_sample, sample);

    // Close the device file
    close(fd);

    return 0;
}