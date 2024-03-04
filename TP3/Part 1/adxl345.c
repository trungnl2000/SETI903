#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>


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

// A global variable indicating the number of accelerometers present (TP3)
static int num_accelerometers = 0;

// Declare a struct adxl345_device structure containing for the moment a single struct miscdevice field (TP3)
struct adxl345_device {
    struct miscdevice miscdev;
};

static int adxl345_probe(struct i2c_client *client)
{
    // TP2
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

    // TP3
    // Declaration of variable
    int ret;
    struct adxl345_device *adxl345_dev;
    char *name;
    // Dynamically allocate memory for the adxl345_device instance
    adxl345_dev = kzalloc(sizeof(*adxl345_dev), GFP_KERNEL);
    if (!adxl345_dev)
        return -ENOMEM; //Out of Memory error

    // Associate the instance with the i2c_client
    i2c_set_clientdata(client, adxl345_dev);

    // Generate unique name
    name = kasprintf(GFP_KERNEL, "adxl345-%d", num_accelerometers++);
    if (!name) {
        kfree(adxl345_dev);
        return -ENOMEM; //Out of Memory error
    }

    
    // Fill the content of the miscdevice structure
    adxl345_dev->miscdev.minor = MISC_DYNAMIC_MINOR; // dynamically assign a minor number
    adxl345_dev->miscdev.name = name;
    adxl345_dev->miscdev.fops = NULL; // No fops at the moment

    // Register with the misc framework
    ret = misc_register(&adxl345_dev->miscdev);
    if (ret) {
        pr_info("Failed to register %s\n", adxl345_dev->miscdev.name);
        kfree(adxl345_dev);
        return ret;
    }

    // Free the name as it's no longer needed
    kfree(name);

    pr_info("Successfully registered %s\n", adxl345_dev->miscdev.name);
    

    return 0;
}


static void adxl345_remove(struct i2c_client *client)
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