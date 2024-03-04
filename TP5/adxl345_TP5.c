#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/kfifo.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/mutex.h>



// Reference from https://www.analog.com/media/en/technical-documentation/data-sheets/adxl345.pdf (TP2)
// Define the register addresses of adxl345: (TP2)
#define ADXL345_REG_BW_RATE             0x2C
#define ADXL345_REG_INT_ENABLE          0x2E
#define ADXL345_REG_DATA_FORMAT         0x31
#define ADXL345_REG_FIFO_CTL            0x38
#define ADXL345_REG_POWER_CTL           0x2D

// Define their values (TP2)
#define ADXL345_OUTPUT_RATE_100HZ       0x0A
#define ADXL345_ALL_INTERRUPTS_DISABLED 0x00
#define ADXL345_DATA_FORMAT_DEFAULT     0x00
#define ADXL345_FIFO_BYPASS_MODE        0x00
#define ADXL345_MEASURE_MODE            0x08
#define ADXL345_STANDBY_MODE            0x00

#define ADXL345_DATAX0     0x32
#define ADXL345_DATAX1     0x33
#define ADXL345_DATAY0     0x34
#define ADXL345_DATAY1     0x35
#define ADXL345_DATAZ0     0x36
#define ADXL345_DATAZ1     0x37

// A global variable indicating the number of accelerometers present (TP3)
static int num_accelerometers = 0;


// TP4
#define ADXL345_REG_FIFO_STATUS 0x39

struct fifo_element {
    // Structure representing a sample from the accelerometer
    s16 x;
    s16 y;
    s16 z;
};



// Declare a struct adxl345_device structure containing for the moment a single struct miscdevice field (TP3)
struct adxl345_device {
    struct miscdevice miscdev;
    // Create FIFO to store samples (TP4)
    DECLARE_KFIFO(samples_fifo, struct fifo_element, 64); // Arbitrary size, adjust as needed
    // Declare the queue
    wait_queue_head_t wait_queue;

    struct mutex lock; // Declare the mutex lock
};


// Custom IOCTL commands (TP3 - part3)
#define ADXL_IOCTL_SET_AXIS_X _IO('X', 0)
#define ADXL_IOCTL_SET_AXIS_Y _IO('Y', 1)
#define ADXL_IOCTL_SET_AXIS_Z _IO('Z', 2)

static int current_axis = ADXL_IOCTL_SET_AXIS_X;  // Default axis is X

static long adxl345_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
        case ADXL_IOCTL_SET_AXIS_X:
        case ADXL_IOCTL_SET_AXIS_Y:
        case ADXL_IOCTL_SET_AXIS_Z:
            current_axis = cmd;
            return 0;
        default:
            return -ENOTTY;  // Not a valid ioctl command
    }
}


// Function to read data from the accelerometer
static ssize_t adxl345_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{   
    struct adxl345_device *adxl_dev;
    struct i2c_client *client;
    ssize_t ret;


    // Retrieve the instance of the struct adxl345_device
    adxl_dev = container_of(file->private_data, struct adxl345_device, miscdev);

    mutex_lock(&adxl_dev->lock); // Acquire the mutex lock

    // Retrieve the instance of the struct i2c_client
    client = to_i2c_client(adxl_dev->miscdev.parent);

    // Define the address of register axis
    char reg_data_address;
    switch (current_axis) {
        case ADXL_IOCTL_SET_AXIS_X:
            reg_data_address = 'X';
            break;
        case ADXL_IOCTL_SET_AXIS_Y:
            reg_data_address = 'Y';
            break;
        case ADXL_IOCTL_SET_AXIS_Z:
            reg_data_address = 'Z';
            break;
        default:
            return -EINVAL;  // Invalid argument
    }



    // Check if data is available in the FIFO, if not, put the process in to wait
    if(kfifo_is_empty(&adxl_dev->samples_fifo))
        printk("FIFO is empty!!\n");
    wait_event_interruptible(adxl_dev->wait_queue, !kfifo_is_empty(&adxl_dev->samples_fifo));

    // Data available in the FIFO, retrieve it
    struct fifo_element sample;
    ret = kfifo_get(&adxl_dev->samples_fifo, &sample);

    s16 accel_data;

    if(reg_data_address == 'X')
        accel_data = sample.x;
    else if (reg_data_address == 'Y')
        accel_data = sample.y;
    else if (reg_data_address == 'Z')
        accel_data = sample.z;
    else {
        pr_err("Wrong axis!!!!!\n");
        return -EIO;
        }


    // Pass all or part of this sample to the application
    if (count >= sizeof(accel_data)) {
        count = sizeof(accel_data);
    }

    if (copy_to_user(buf, &accel_data, count)) {
        // Error while copying data to user space
        return -EFAULT;
    }

    mutex_unlock(&adxl_dev->lock); // Release the mutex lock
    
    return count;

}

// Write the bottom half function ( adxl345_int for example):
static irqreturn_t adxl345_int(int irq, void *dev_id)
{
    pr_info("This is interupt handle\n");
    struct adxl345_device *adxl_dev = dev_id;
    // Convert the pointer to `adxl_dev->miscdev.parent` into a pointer to `client` (Make adxl_dev->miscdev.parent have type struct i2c_client)
    struct i2c_client *client = to_i2c_client(adxl_dev->miscdev.parent);

    u8 fifo_status_reg = ADXL345_REG_FIFO_STATUS;
    u8 fifo_status;
    int ret;

    // Read FIFO status register to determine the number of samples available
    ret = i2c_master_send(client, &fifo_status_reg, 1);
    if (ret != 1) {
        pr_err("Failed to send command to read FIFO status\n");
        return ret; // or appropriate error handling
    }
    ret = i2c_master_recv(client, &fifo_status, 1);
    if (ret != 1) {
        pr_err("Failed to read FIFO status\n");
        return ret; // or appropriate error handling
    }

    // Check FIFO status to determine the number of samples available
    int num_samples = fifo_status & 0x1F; // Bits 0-4 represent the number of samples
    pr_info("Number of samples available in FIFO: %d\n", num_samples);

    // Read samples from the accelerometer FIFO
    u8 reg_data_address[] = {ADXL345_DATAX0, ADXL345_DATAX1, ADXL345_DATAY0, ADXL345_DATAY1, ADXL345_DATAZ0, ADXL345_DATAZ1};
    i2c_master_send(client, reg_data_address, 6);

    // Allocate memory for reg_data dynamically
    int num_byte_read = num_samples * 3 * 2 * sizeof(u8); // Each sample contains 2 bytes of data from 3 axis
    u8 *reg_data = kmalloc(num_byte_read, GFP_KERNEL);
    if (!reg_data) {
        pr_err("Failed to allocate memory for reg_data\n");
        return IRQ_HANDLED; // or appropriate error handling
    }

    // Retrieve all samples from the accelerometer FIFO
    i2c_master_recv(client, reg_data, num_byte_read);
    int i;
    for (i = 0; i < num_byte_read; i += 6) { // Travel through each sample by increasing the index by 6 (bytes) each time
        struct fifo_element sample;
        // Get X-axis data from reg_data
        sample.x = (s16)(reg_data[i + 1] << 8) | reg_data[i];
        // Get Y-axis data from reg_data
        sample.y = (s16)(reg_data[i + 3] << 8) | reg_data[i + 2];
        // Get Z-axis data from reg_data
        sample.z = (s16)(reg_data[i + 5] << 8) | reg_data[i + 4];

        printk("FIFO's data of %d sample is: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n", i/6, reg_data[i], reg_data[i + 1], reg_data[i + 2], reg_data[i + 3],
        reg_data[i + 4], reg_data[i + 5]);

        ret = kfifo_put(&adxl_dev->samples_fifo, sample);
    }

    // Free the dynamic array
    kfree(reg_data);

    // Wake up processes waiting for data
    wake_up_interruptible(&adxl_dev->wait_queue);

    // Delay for 10 seconds (10000 milliseconds)
    msleep(10000);

    return IRQ_HANDLED;
}




static int adxl345_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    /////////////////////////// TP2 ///////////////////////////
    // Declaration of variables
    u8 reg_data[2];
    int ret_arr[5];
    int i;
    ///////////////////////////////////////////////////////
    // Configure I2C messages
    // Define output data rate
    reg_data[0] = ADXL345_REG_BW_RATE;
    reg_data[1] = ADXL345_OUTPUT_RATE_100HZ;
    ret_arr[0] = i2c_master_send(client, reg_data, 2);
    // All interrupts disabled
    reg_data[0] = ADXL345_REG_INT_ENABLE;
    reg_data[1] = ADXL345_ALL_INTERRUPTS_DISABLED;
    ret_arr[1] = i2c_master_send(client, reg_data, 2);

    // Default data format
    reg_data[0] = ADXL345_REG_DATA_FORMAT;
    reg_data[1] = ADXL345_DATA_FORMAT_DEFAULT;
    ret_arr[2] = i2c_master_send(client, reg_data, 2);

    // FIFO bypass
    reg_data[0] = ADXL345_REG_FIFO_CTL;
    reg_data[1] = ADXL345_FIFO_BYPASS_MODE;
    ret_arr[3] = i2c_master_send(client, reg_data, 2);

    // Measurement mode activated
    reg_data[0] = ADXL345_REG_POWER_CTL;
    reg_data[1] = ADXL345_MEASURE_MODE;
    ret_arr[4] = i2c_master_send(client, reg_data, 2);

    for (i = 0; i < 5; i++){
        if(ret_arr[i] != 2){
            printk("Failed to probe ADXL345\n");
            return ret_arr[i];
        }
    }
    
    pr_info("Successfully probe!\n\n");

    /////////////////////////// TP3 ///////////////////////////
    // Declaration of variable
    int ret;
    // Dynamically allocate memory for the adxl345_device instance
    struct adxl345_device *adxl345_dev = kzalloc(sizeof(*adxl345_dev), GFP_KERNEL);
    if (!adxl345_dev)
        return -ENOMEM; //Out of Memory error

    // Associate this instance with the struct i2c_client
    adxl345_dev->miscdev.parent = &client->dev;

    char *name;
    // Generate unique name
    name = kasprintf(GFP_KERNEL, "adxl345-%d", num_accelerometers++);
    if (!name) {
        kfree(adxl345_dev);
        return -ENOMEM; //Out of Memory error
    }

    // Declare the read function in the file operations structure
    static const struct file_operations adxl345_fops = {
        .owner = THIS_MODULE,
        .read = adxl345_read,
    };
    // Fill the content of the miscdevice structure
    adxl345_dev->miscdev.minor = MISC_DYNAMIC_MINOR; // dynamically assign a minor number
    adxl345_dev->miscdev.name = name;
    adxl345_dev->miscdev.fops = &adxl345_fops; // No fops at the moment

    // Register with the misc framework
    ret = misc_register(&adxl345_dev->miscdev);
    if (ret) {
        pr_info("Failed to register %s\n", adxl345_dev->miscdev.name);
        kfree(adxl345_dev);
        return ret;
    }

    // Free the name as it's no longer needed
    kfree(name);

    // Associate the instance with the i2c_client
    i2c_set_clientdata(client, adxl345_dev);

    pr_info("Successfully registered %s\n", adxl345_dev->miscdev.name);
    /////////////////////////// TP4 ///////////////////////////
    // Configure the accelerometer correctly (registers INT_ENABLE and FIFO_CTL)
    // Enable Watermark interrupt in INT_ENABLE register
    reg_data[0] = ADXL345_REG_INT_ENABLE;
    reg_data[1] = 0x02; //1 << 2; // Watermark interrupt bit (00000010)
    ret = i2c_master_send(client, reg_data, 2);
    if (ret != 2) {
        pr_err("Failed to enable Watermark interrupt\n");
        return ret;
    }

    // Configure FIFO_CTL register to enable FIFO mode and set watermark level to 20
    reg_data[0] = ADXL345_REG_FIFO_CTL;
    reg_data[1] = 0x94; //(1 << 7) | 20; // FIFO Stream mode enabled, watermark level set to 20 (10000000 OR 00010100)
    ret = i2c_master_send(client, reg_data, 2);
    if (ret != 2) {
        pr_err("Failed to configure FIFO_CTL register\n");
        return ret;
    }

    // Register a function as a bottom half to handle interrupts with the Threaded IRQ mechanism
    ret = devm_request_threaded_irq(&client->dev, client->irq, NULL, adxl345_int, IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "adxl345_int", adxl345_dev);
    if (ret) {
        pr_err("Failed to register IRQ handler\n");
        return ret;
    }

    // Initialize FIFO
    INIT_KFIFO(adxl345_dev->samples_fifo);

    // Initialize the queue
    init_waitqueue_head(&adxl345_dev->wait_queue);
    
    pr_info("Successfully probe TP4\n");

    mutex_init(&adxl345_dev->lock); // Initialize the mutex lock
    
    
    return 0;
}


static int adxl345_remove(struct i2c_client *client)
{
    // TP2
    u8 reg_data[2];
    int ret;
    // Switch to standby mode in POWER_CTL register
    reg_data[0] = ADXL345_REG_POWER_CTL;
    reg_data[1] = ADXL345_STANDBY_MODE;

    ret = i2c_master_send(client, reg_data, 2);
    if (ret != 2){
        printk("Failed to switch to standby mode !!\n");
        return ret;
    }

    // TP3
    // Retrieve the instance of the struct adxl345_device from the struct i2c_client retrieved as argument
    struct adxl345_device *adxl345_dev = i2c_get_clientdata(client);
    // Unregister from the misc framework
    misc_deregister(&adxl345_dev->miscdev);

    // Decrement the number of accelerometers
    num_accelerometers--;

    pr_info("%s misc device unregistered successfully\n", adxl345_dev->miscdev.name);

    kfree(adxl345_dev);
    //
    pr_info("Successfully remove!\n\n");
    return 0;
}


static struct i2c_device_id adxl345_idtable[] = {
    { "adxl345", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, adxl345_idtable);

#ifdef CONFIG_OF
static const struct of_device_id adxl345_of_match[] = {
    {   .compatible = "qemu,adxl345",
        .data       = NULL },
    {}
};

MODULE_DEVICE_TABLE(of, adxl345_of_match);
#endif

static struct i2c_driver adxl345_driver = {
        .driver = {
        .name           = "adxl345",
        .of_match_table = of_match_ptr(adxl345_of_match),
    },
    .id_table   = adxl345_idtable,
    .probe      = adxl345_probe,
    .remove     = adxl345_remove,
};

module_i2c_driver(adxl345_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("adxl345 driver");
MODULE_AUTHOR("Le-Trung NGUYEN");