/*
 * TODO: 
 * [x] Basic driver to initialize seesaw soil sensor
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
#include <linux/delay.h>

/* Seesaw Module Base Addresses */
#define SEESAW_STATUS_BASE	0x00
#define SEESAW_GPIO_BASE	0x01
#define SEESAW_SERCOMO_BASE 	0x02

/* Function Addresses */
#define SEESAW_STATUS_HW_ID	0x01
#define SEESAW_STATUS_VERSION	0x02
#define SEESAW_STATUS_OPTIONS	0x03
#define SEESAW_STATUS_TEMP	0x04
#define SEESAW_TOUCH_CHANNEL	0x0F

#define SEESAW_HW_ID_CODE	0x55	/* Expected hardware ID */
#define	SEESAW_READING_DELAY	1000	/* Time in ms between readings */


#define SEESAW_I2C_ADDRESS 0x36 	/* Default I2C address for Seesaw */
#define SEESAW_STATUS_REG 0x00
#define SEESAW_ADC_CHANNEL_ENABLE 0x07
#define SEESAW_ADC_READ 0x09

/* private structre to store device-specific information */
struct seesaw_dev {
	struct i2c_client *client;
	struct miscdevice seesaw_miscdevice;
	struct device *dev;
	struct mutex lock;
	char name[8]; /* stemmaXX */
};

/* Read the seesaw register */
/*static int seesaw_read_reg(struct seesaw_dev *seesaw, u8 reg, u8 *buf, size_t len)*/
static int seesaw_read_reg(struct seesaw_dev *seesaw, u8 reg_base, u8 reg_addr,
				u8 *buf, size_t len)
{
	pr_info("Entering the seesaw_read_reg function\n");
	u8 reg_buf[2] = {reg_base, reg_addr};  /*added for version 2 */
	struct i2c_msg msg[2] = {
		{
			.addr = seesaw->client->addr,
			.flags = 0,
			.len = 2, /* 1 */
			.buf = reg_buf, /* &reg,*/
		},	
		{
			.addr = seesaw->client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};
	pr_info("Exiting the seesaw_read_reg function\n");
	return i2c_transfer(seesaw->client->adapter, msg, 2);
}

/* Write a register on the Seesaw sesnor */
/*static int seesaw_write_reg(struct seesaw_dev *seesaw, u8 reg, u8 value)*/
static int seesaw_write_reg(struct seesaw_dev *seesaw, u8 reg_base,
				u8 reg_addr, u8 *buf, int len)
{
	pr_info("Entering the seesaw_write_reg function\n");
	u8 write_buf[2] = {reg_base, reg_addr}; /*{reg, value};*/
	memcpy(&write_buf[2], buf, len);
	struct i2c_msg msg = {
		.addr = seesaw->client->addr,
		.flags = 0,
		.len = 2, /*2*/
		.buf = write_buf, /*buf,*/
	};
	pr_info("exiting the seesaw_write_reg function\n");
	if (i2c_transfer(seesaw->client->adapter, &msg, 1)  < 0){
		pr_info("Failed to write register\n");
		return -121;
	}

	return 0;
}

/* Read the soil temperature for the seesaw soil sesnor */
static int seesaw_read_temperature(struct seesaw_dev *seesaw)
{
	pr_info("Entering the seesaw_write_reg functin\n");
	u8 buf[2];
	int ret;

	/*mutex_lock(&seesaw->lock);*/
	/*ret = seesaw_write_reg(seesaw, SEESAW_ADC_CHANNEL_ENABLE, 0x02);*/
	ret = seesaw_write_reg(seesaw, SEESAW_STATUS_BASE, SEESAW_STATUS_TEMP, buf, 2);
	/*if (ret < 0){
		pr_info("seesaw_write_reg failed with value: %d\n", ret);
		return ret;
	}*/

	/* Dealy to complete read */
	msleep(10);

	/* Read ADC value */
	/*ret = seesaw_read_reg(seesaw, SEESAW_ADC_READ, buf, 2); */ 
	ret = seesaw_read_reg(seesaw, SEESAW_STATUS_BASE, SEESAW_STATUS_TEMP, buf, 2);
	if (ret < 0){
		pr_info("TEMPERATURE: seesaw_read_reg failed with value: %d\n", ret);
		return ret;
	}
	else
		pr_info("the return value: %d\n", ret);
	/*mutex_unlock(&seesaw->lock);*/

	pr_info("Exiting the seesaw_read_temperatue function\n");
	return (buf[0] << 8) | buf[1];
} 

/*static int seesaw_read_moisture(struct seesaw_dev *seesaw)
{
	pr_info("Entring the seesaw_read_moisture function\n");
	u8 buf[2];
	int ret;

	mutex_lock(&seesaw->lock);
	 Enable ADC channel 0 (moisture)
	ret = seesaw_write_reg(seesaw, SEESAW_ADC_CHANNEL_ENABLE, 0x01); 
	if (ret < 0) return ret; 

	msleep(10);

	Read ADC value 
	ret = seesaw_read_reg(seesaw, SEESAW_ADC_READ, buf, 2);
	if (ret < 0) return ret;
	mutex_unlock(&seesaw->lock);

	pr_info("Entering the seesaw_read_moisture function\n");	
	return (buf[0] << 8) | buf[1];
}*/

/* User reading data from /dev/stemma */
static ssize_t seesaw_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	pr_info("stemma_fops reading function\n");

	int moisture = 0, temperature, len;
	char data[32]; 
	struct seesaw_dev *seesaw; 

	/* Ensure file->private_data is valid */
	if (!file->private_data){
		pr_info("file->private_data is NULL\n");
		return -EFAULT;
	}

	seesaw = container_of(file->private_data, struct seesaw_dev, seesaw_miscdevice);
	

	/*moisture = seesaw_read_moisture(seesaw); */
	temperature = seesaw_read_temperature(seesaw);

	/* Format the data */
	len = snprintf(data, sizeof(data), "Moisture: %d, Temperature: %d\n", moisture, temperature);
	
	pr_info("This is the sensor data: %s\n", data); 
	
	/* Copy data to userspace */
	if (count < len) return -EINVAL; /* Ensure buffer is large enough */
	if (copy_to_user(buf, data, 32)){
		return -EFAULT;
	}
	pr_info("Exiting seesaw_read function\n");
	return len;
}

/* Called whenever a suser space write occurs on one of the character devices */
static ssize_t seesaw_write(struct file *file, const char __user *userbuf,
				size_t count, loff_t *ppos)
{
	pr_info("stemma_fops write function\n");

	int ret; 
	unsigned long val;
	char buf[4]; 
	struct seesaw_dev *seesaw;
	
	/* misdevice struct is accessible through 'file->private_data'
	 * and is a member of the stemma_dev struct, you can use the 
 	 * 'container_of()' macro to computer the address of the
 	 * private struct and revocer the i2c_client struct form it.
	 */
	seesaw = container_of(file->private_data, struct seesaw_dev, seesaw_miscdevice);

	if (copy_from_user(buf, userbuf, count)){
		pr_info("Bad copied value\n");
	}
	
	/* Convert char array to char string*/
	buf[count-1] = '\0';

	/* Convert the string to an unsigned long */
	ret = kstrtoul(buf, 0, &val);

	/* write the data to the I2C stemma device*/
	i2c_smbus_write_byte(seesaw->client, val); 

	pr_info("Exiting the seesaw_write function\n");

	return count; 
}


/* file operations to define which driver function are called when 
 * the user reads and writes to the device
 */
static const struct file_operations seesaw_fops = {
	.owner 	=	THIS_MODULE,
	.read 	=	seesaw_read,
	.write 	=	seesaw_write,
};

static int seesaw_probe(struct i2c_client *client)
{

	pr_info("Entering stemma probe function\n");
	static int counter = 0;
	struct seesaw_dev *seesaw;

	if(client->addr == 0x36)
		pr_info("We go the right device\n");
	else
		pr_info("We do not have the right device\n");

	/* Allocate the stemma private structure */
	seesaw = devm_kzalloc(&client->dev, sizeof(struct seesaw_dev), GFP_KERNEL);
	
	/* Store pointer to the device-structure in the bus device context */
	i2c_set_clientdata(client, seesaw);

	/* Store pointer to I2C client */
	seesaw->client = client;

	/*
	 * Initialize the misc device, stemma is incremented 
	 * after each probe call
	 */
	sprintf(seesaw->name, "seesaw%02d", counter++);
	dev_info(&client->dev, "stemma_probe is entered on %s\n", seesaw->name);

	seesaw->seesaw_miscdevice.name = seesaw->name;
	seesaw->seesaw_miscdevice.minor = MISC_DYNAMIC_MINOR; 
	seesaw->seesaw_miscdevice.fops = &seesaw_fops;

	dev_info(&client->dev, "seesaw_probe is exited on %s\n", seesaw->name);

	/* Register misc device */
	return misc_register(&seesaw->seesaw_miscdevice);
	return 0;	
	/*dev_info(&client->dev, "seesaw_probe is exited on %s\n", seesaw->name);*/
}

static void seesaw_remove(struct i2c_client *client)
{
	pr_info("Entering the stemma remove function\n");
	struct seesaw_dev *seesaw ;

	/* Get device structure from bus device context */
	seesaw = i2c_get_clientdata(client);

	/* Deregister misc device */
	misc_deregister(&seesaw->seesaw_miscdevice);

	pr_info("Exiting the stemma remove function\n");
} 

/* list of devices supported by the driver */
static const struct of_device_id seesaw_dt_ids[] = {
	{ .compatible = "arrow,stemma", },
	{}
};
MODULE_DEVICE_TABLE(of, seesaw_dt_ids);

/* an array of i2c_device_id structures */
static const struct i2c_device_id i2c_ids[] = {
	{ .name = "stemma", },
	{ }
};
MODULE_DEVICE_TABLE(i2c, i2c_ids);

/* i2c_driver structure that will be registered to the I2C bus */
static struct i2c_driver seesaw_driver = {
	.probe	= 	seesaw_probe,
	.remove	=	seesaw_remove,
	.id_table =	i2c_ids,
	.driver = {
		.name	=	"stemma",
		.owner	=	THIS_MODULE,	
		.of_match_table = seesaw_dt_ids
	},
};
/* register the driver with the I2C bus */
module_i2c_driver(seesaw_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Erastus Toe <chukpozohnt@gmil.com>");
MODULE_DESCRIPTION("This driver controls the Adafruit STEMMA Soil Sensor");
