#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/i2c.h>

// Reference from https://www.analog.com/media/en/technical-documentation/data-sheets/adxl345.pdf
// Define the register addresses of adxl345: 
#define ADXL345_REG_BW_RATE             0x2C
#define ADXL345_REG_INT_ENABLE          0x2E
#define ADXL345_REG_DATA_FORMAT         0x31
#define ADXL345_REG_FIFO_CTL            0x38
#define ADXL345_REG_POWER_CTL           0x2D

// Define their values
#define ADXL345_OUTPUT_RATE_100HZ       0x0A
#define ADXL345_ALL_INTERRUPTS_DISABLED 0x00
#define ADXL345_DATA_FORMAT_DEFAULT     0x00
#define ADXL345_FIFO_BYPASS_MODE        0x00
#define ADXL345_MEASURE_MODE            0x08
#define ADXL345_STANDBY_MODE            0x00


static int adxl345_probe(struct i2c_client *client, const struct i2c_device_id *id)
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

////////////////////////// Read the configured register value to check if they are correct ///////////////////////////
    // For checking
    int ret;
    u8 reg_addr;
    u8 register_addresses[] = {
        ADXL345_REG_BW_RATE,
        ADXL345_REG_INT_ENABLE,
        ADXL345_REG_DATA_FORMAT,
        ADXL345_REG_FIFO_CTL,
        ADXL345_REG_POWER_CTL
    };
    u8 register_values[sizeof(register_addresses)];
    const char *register_names[] = {
        "ADXL345_REG_BW_RATE",
        "ADXL345_REG_INT_ENABLE",
        "ADXL345_REG_DATA_FORMAT",
        "ADXL345_REG_FIFO_CTL",
        "ADXL345_REG_POWER_CTL"
    };

    pr_info("Checking the configured register value in the probe function:\n");
    for (i = 0; i < sizeof(register_addresses); i++) {
        reg_addr = register_addresses[i];
        ret = i2c_master_send(client, &reg_addr, 1);
        if (ret != 1) {
            printk("Failed to write to register 0x%02X\n", reg_addr);
            return ret;
        }

        ret = i2c_master_recv(client, &register_values[i], 1);
        if (ret != 1) {
            printk("Failed to read from register 0x%02X\n", reg_addr);
            return ret;
        }

        pr_info("Register %s with address 0x%02X has value 0x%02X\n", register_names[i], reg_addr, register_values[i]);
    }
    return 0;
}


static int adxl345_remove(struct i2c_client *client)
{
    u8 reg_data[2];
    int ret;
    // Switch to standby mode in POWER_CTL register
    reg_data[0] = ADXL345_REG_POWER_CTL;
    reg_data[1] = ADXL345_STANDBY_MODE;

    ret = i2c_master_send(client, reg_data, 2);
    if (ret != 2){
        printk("Failed to switch to standby mode !!\n");
        return -EIO;
    }
    pr_info("Successfully remove!\n\n");
////////////////////////// Read the configured register value to check if they are correct ///////////////////////////
    u8 reg_addr, reg_value;

    pr_info("Checking the configured register value in the remove function:\n");
    reg_addr = ADXL345_REG_POWER_CTL;
    ret = i2c_master_send(client, &reg_addr, 1);
    if (ret != 1) {
        printk("Failed to write to ADXL345_REG_POWER_CTL\n");
        return ret;
    }
    // Read one byte from the register
    ret = i2c_master_recv(client, &reg_value, 1);
    if (ret != 1) {
        printk("Failed to read from ADXL345_REG_POWER_CTL\n");
        return ret;
    }

    pr_info("Register ADXL345_REG_POWER_CTL with address 0x%02X has value 0x%02X\n", reg_addr, reg_value);
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