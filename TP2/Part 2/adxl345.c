#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/i2c.h>

#define DEVID_REG 0x00


static int adxl345_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret;
    u8 reg_buf[1] = {DEVID_REG};
    u8 devid;
    ret = i2c_master_send(client, reg_buf, 1);
    if(ret!=1){
        dev_err(&client-> dev, "Failed to write DEVID register address\n");
        return ret;
    }

    ret = i2c_master_recv(client, &devid, 1);
    if(ret!=1){
        dev_err(&client-> dev, "Failed to read DEVID register\n");
        return ret;
    }

    pr_info("DEVID register value: 0x%02X\n", devid);

    return 0;
}

static int adxl345_remove(struct i2c_client *client)
{
    printk("This is Le-Trung's remove function\n");
    return 0;
}

/* The following list allows the association between a device and its driver
driver in the case of a static initialization without using
device tree.
Each entry contains a string used to make the association
association and an integer that can be used by the driver to
driver to perform different treatments depending on the physical
the physical device detected (case of a driver that can manage
different device models).*/

static struct i2c_device_id adxl345_idtable[] = {
    { "adxl345", 0 },
    { }
};

MODULE_DEVICE_TABLE(i2c, adxl345_idtable);

#ifdef CONFIG_OF
/* If device tree support is available, the following list
allows to make the association using the device tree.
Each entry contains a structure of type of_device_id. The field
compatible field is a string that is used to make the association
with the compatible fields in the device tree. The data field is
a void* pointer that can be used by the driver to perform different
perform different treatments depending on the physical device detected.
device detected.*/
static const struct of_device_id adxl345_of_match[] = {
    {   .compatible = "vendor,adxl345", 
        .data = NULL },
    {}
};
MODULE_DEVICE_TABLE(of, adxl345_of_match);
#endif

static struct i2c_driver adxl345_driver = {
    .driver = {
        /* The name field must correspond to the name of the module
        and must not contain spaces. */
        .name = "adxl345",
        .of_match_table = of_match_ptr(adxl345_of_match),
    },
    .id_table = adxl345_idtable,
    .probe = adxl345_probe,
    .remove = adxl345_remove,
};

module_i2c_driver(adxl345_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("adxl345 driver");
MODULE_AUTHOR("Le-Trung NGUYEN");
