#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>

MODULE_LICENSE("Dual BSD/GPL");

static int hello_init(void)
{
  printk(KERN_ALERT "Hello, world\n");
  return 0;
}

static void hello_exit(void)
{
  printk(KERN_ALERT "Goodbye, cruel world\n");
}

module_init(hello_init);
module_exit(hello_exit);

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
 *      Eventually you fill out i2c_baord_info struct and initiali
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
