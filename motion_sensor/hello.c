#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#define APDS9960_ADDR 0x39
#define PROX_DATA_REG 0x9C

struct apds9960_data {
    struct i2c_client *client;
    struct mutex lock;
};

// Read proximity data
static u8 apds9960_read_proximity(struct i2c_client *client)
{
    u8 data;
    data = i2c_smbus_read_byte_data(client, PROX_DATA_REG);
    return data;
}

// Sysfs attribute to read proximity
static ssize_t proximity_show(struct device *dev,
                            struct device_attribute *attr,
                            char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    u8 prox_data = apds9960_read_proximity(client);
    
    return sprintf(buf, "%d\n", prox_data);
}

// Create the sysfs attribute
static DEVICE_ATTR_RO(proximity);

// Create attribute group
static struct attribute *apds9960_attributes[] = {
    &dev_attr_proximity.attr,
    NULL
};

static const struct attribute_group apds9960_attr_group = {
    .attrs = apds9960_attributes,
};

static int apds9960_probe(struct i2c_client *client)
{
    struct apds9960_data *data;
    int ret;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
        return -EIO;

    data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->client = client;
    mutex_init(&data->lock);
    i2c_set_clientdata(client, data);

    // Register sysfs group instead of individual file
    ret = sysfs_create_group(&client->dev.kobj, &apds9960_attr_group);
    if (ret)
        return ret;

    dev_info(&client->dev, "APDS-9960 probe successful\n");
    return 0;
}

static void apds9960_remove(struct i2c_client *client)
{
    sysfs_remove_group(&client->dev.kobj, &apds9960_attr_group);
}

static const struct i2c_device_id apds9960_id[] = {
    { "apds9960", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, apds9960_id);

#ifdef CONFIG_OF
static const struct of_device_id apds9960_of_match[] = {
    { .compatible = "avago,apds9960" },
    { }
};
MODULE_DEVICE_TABLE(of, apds9960_of_match);
#endif

static struct i2c_driver apds9960_driver = {
    .driver = {
        .name = "apds9960",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(apds9960_of_match),
    },
    .probe = apds9960_probe,
    .remove = apds9960_remove,
    .id_table = apds9960_id,
};

module_i2c_driver(apds9960_driver);

MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("APDS-9960 proximity sensor driver");
MODULE_LICENSE("GPL");

// 
// 
// static int hello_init(void)
// {
//   printk(KERN_ALERT "Hello, world\n");
//   return 0;
// }
// 
// static void hello_exit(void)
// {
//   printk(KERN_ALERT "Goodbye, cruel world\n");
// }
// 
// module_init(hello_init);
// module_exit(hello_exit);

/*
 * Client Driver Game Plan
 * -----------------------
 * See https://www.kernel.org/doc/html/v5.4/i2c/writing-clients.html
 *
 * Setup i2c_device_id
 * Setup i2c_driver
 * Setup extra client data, which can store cutsom internal structures
 * Setup read/write functions using the generic i2c.h functions
 * Probe for and attach the device
 *      This will require some interfacing with the underlying i2c chip driver,
 *      as well as sort of strategy for detecting the i2c device address and whatnot.
 *      Eventually you fill out i2c_baord_info struct and initialize
 * Initialize the driver
 * Handling special states:
 *      low power
 *      shutdown
 *      etc.
 */

/**
 * struct i2c_board_info - template for device creation
 * @type: chip type, to initialize i2c_client.name
 * @flags: to initialize i2c_client.flags
 * @addr: stored in i2c_client.addr
 * @dev_name: Overrides the default <busnr>-<addr> dev_name if set
 * @platform_data: stored in i2c_client.dev.platform_data
 * @of_node: pointer to OpenFirmware device node
 * @fwnode: device node supplied by the platform firmware
 * @swnode: software node for the device
 * @resources: resources associated with the device
 * @num_resources: number of resources in the @resources array
 * @irq: stored in i2c_client.irq
 *
 * I2C doesn't actually support hardware probing, although controllers and
 * devices may be able to use I2C_SMBUS_QUICK to tell whether or not there's
 * a device at a given address.  Drivers commonly need more information than
 * that, such as chip type, configuration, associated IRQ, and so on.
 *
 * i2c_board_info is used to build tables of information listing I2C devices
 * that are present.  This information is used to grow the driver model tree.
 * For mainboards this is done statically using i2c_register_board_info();
 * bus numbers identify adapters that aren't yet available.  For add-on boards,
 * i2c_new_client_device() does this dynamically with the adapter already known.
 */
// struct i2c_board_info {
// 	char		type[I2C_NAME_SIZE];
// 	unsigned short	flags;
// 	unsigned short	addr;
// 	const char	*dev_name;
// 	void		*platform_data;
// 	struct device_node *of_node;
// 	struct fwnode_handle *fwnode;
// 	const struct software_node *swnode;
// 	const struct resource *resources;
// 	unsigned int	num_resources;
// 	int		irq;
// };
