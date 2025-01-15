/*
 * TODO: 
 * [x] Basic driver to initialize stemma soil sensor
 * [] Implement fops read & write function & test driver
 */

/*
 * ********** BOARD INFORMATION ***********
 * Chip Family:		SAMD20
 * Chip Cariant: 	SAMD10D14AM
 * Board Name:		SOIL
 */

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/uaccess.h>

#define	STEMMA_TOUCH_REG 0x0F /* Moisture data */
#define STEMMA_TEMP_REG 0x10 /* Temperature data (2 bytes) */
#define STEMMA_MEASURE_CMD 0X0C /* Trigger measurement */

/*#define SOIL_TEMP_REG 0X01
#define SOIL_HUMIDITY_REG 0X01
#define SOIl_MOISTURE_REG 0X02
#define SOIL_I2C_ADDR 0x36 */

/* private structre to store device-specific information */
struct stemma_dev {
	struct i2c_client *client;
	struct miscdevice stemma_miscdevice;
	char name[8]; /* stemmaXX */
};

/* User reading data from /dev/stemma */
static ssize_t stemma_read_file(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	pr_info("stemma_fops reading function\n");

	u8 stemma_base_reg = 0x02; /* stemma soil sensor base register */
	int size;
	uint16_t moisture, temperature;
	char buf[32];
	struct stemma_dev *stemma; 

	stemma = container_of(file->private_data, struct stemma_dev, stemma_miscdevice);

	/* Write to the measurement trigger register */

	/* Read moisture data */
	moisture  = i2c_smbus_read_byte_data(stemma->client, STEMMA_TOUCH_REG);
	if (moisture  < 0){
		pr_info("Failed to read moisture data\n");
		return -EFAULT;
	}

	/* Read temperature data */
	temperature = i2c_smbus_read_byte_data(stemma->client, STEMMA_TEMP_REG);
	if ( temperature < 0){
		pr_info("Failed tor ead temperature data\n");
		return -EFAULT;
	}
	temperature = ((temperature * 200) / 65536) - 50;
	

	size = sprintf(buf,"Moisture: %02x, Temperature: %02x", moisture, temperature);

	/*
	 * Replace NULL by \n. It is not needed to have a char array
	 * ended with \0 character.
	 */
	buf[size] = '\n';

	/* send size+1 to include the \n character */
	if (*ppos == 0){
		/*if (copy_to_user(userbuf, buf, size+1)){*/
		if (copy_to_user(userbuf, buf, strlen(buf))){
			pr_info("Failed to return led_value to user space\n");
			return -EFAULT;
		}
		*ppos+=1;
		return size+1;
	}
	
	return 0;
}

/* Called whenever a suser space write occurs on one of the character devices */
static ssize_t stemma_write_file(struct file *file, const char __user *userbuf,
				size_t count, loff_t *ppos)
{
	pr_info("stemma_fops write function\n");

	int ret; 
	unsigned long val;
	char buf[4]; 
	struct stemma_dev *stemma;
	
	/* misdevice struct is accessible through 'file->private_data'
	 * and is a member of the stemma_dev struct, you can use the 
 	 * 'container_of()' macro to computer the address of the
 	 * private struct and revocer the i2c_client struct form it.
	 */
	stemma = container_of(file->private_data, struct stemma_dev, stemma_miscdevice);

	if (copy_from_user(buf, userbuf, count)){
		pr_info("Bad copied value\n");
	}
	
	/* Convert char array to char string*/
	buf[count-1] = '\0';

	/* Convert the string to an unsigned long */
	ret = kstrtoul(buf, 0, &val);

	/* write the data to the I2C stemma device*/
	i2c_smbus_write_byte(stemma->client, val); 

	return count; 
}


/* file operations to define which driver function are called when 
 * the user reads and writes to the device
 */
static const struct file_operations stemma_fops = {
	.owner 	=	THIS_MODULE,
	.read 	=	stemma_read_file,
	.write 	=	stemma_write_file,
};

static int stemma_probe(struct i2c_client *client)
{

	pr_info("Entering stemma probe function\n");
	static int counter = 0;
	struct stemma_dev *stemma;

	/* Allocate the stemma private structure */
	stemma = devm_kzalloc(&client->dev, sizeof(struct stemma_dev), GFP_KERNEL);
	
	/* Store pointer tot he device-structure in the bus device context */
	i2c_set_clientdata(client, stemma);

	/* Store pointer to I2C client */
	stemma->client = client;
	
	/*
	 * Initialize the misc device, stemma is incremented 
	 * after each probe call
	 */
	sprintf(stemma->name, "stemma%02d", counter++);
	dev_info(&client->dev, "stemma_probe is entered on %s\n", stemma->name);

	stemma->stemma_miscdevice.name = stemma->name;
	stemma->stemma_miscdevice.minor = MISC_DYNAMIC_MINOR; 
	stemma->stemma_miscdevice.fops = &stemma_fops;

	/* Register misc device */
	return misc_register(&stemma->stemma_miscdevice);
	
	dev_info(&client->dev, "stemma_probe is exited on %s\n", stemma->name);
	return 0;
}

static void stemma_remove(struct i2c_client *client)
{
	pr_info("Entering the stemma remove function\n");
	struct stemma_dev *stemma ;

	/* Get device structure from bus device context */
	stemma = i2c_get_clientdata(client);

	/* Deregister misc device */
	misc_deregister(&stemma->stemma_miscdevice);

	pr_info("Exiting the stemma remove function\n");
} 

/* list of devices supported by the driver */
static const struct of_device_id stemma_dt_ids[] = {
	{ .compatible = "arrow,stemma", },
	{}
};
MODULE_DEVICE_TABLE(of, stemma_dt_ids);

/* an array of i2c_device_id structures */
static const struct i2c_device_id i2c_ids[] = {
	{ .name = "stemma", },
	{ }
};
MODULE_DEVICE_TABLE(i2c, i2c_ids);

/* i2c_driver structure that will be registered to the I2C bus */
static struct i2c_driver stemma_driver = {
	.probe	= 	stemma_probe,
	.remove	=	stemma_remove,
	.id_table =	i2c_ids,
	.driver = {
		.name	=	"stemma",
		.owner	=	THIS_MODULE,	
		.of_match_table = stemma_dt_ids
	},
};
/* register the driver with the I2C bus */
module_i2c_driver(stemma_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Erastus Toe <chukpozohnt@gmil.com>");
MODULE_DESCRIPTION("This driver controls the Adafruit STEMMA Soil Sensor");
