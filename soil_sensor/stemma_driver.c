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

/* private structre to store device-specific information */
struct seesaw_dev {
	struct i2c_client *client;
	struct miscdevice seesaw_miscdevice;
	struct device *dev;
	struct mutex lock;
	char name[8]; /* stemmaXX */
};

/* Read the seesaw register */
static int seesaw_read_reg(struct seesaw_dev *seesaw, u8 reg_base,
			u8 reg_addr, u8 *buf, size_t len)
{

	int ret;
	u8 reg_buf[2] = {reg_base, reg_addr };

	ret = i2c_master_send(seesaw->client, reg_buf, 2);
	if (ret < 0)
		return ret; 

	if (reg_base == SEESAW_STATUS_BASE){
		if(reg_addr == SEESAW_STATUS_TEMP)
		msleep(5);
		else if (reg_addr == SEESAW_TOUCH_CHANNEL)
		usleep_range(1000, 2000);
	}

	ret = i2c_master_recv(seesaw->client, buf, len);
	if (ret < 0)
		return ret;
	return 0;

	/*struct i2c_msg msgs[2];
	u8 reg_buf[2] = {reg_base, reg_addr};
	int ret;

	msgs[0].addr = seesaw->client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = reg_buf;

	msgs[1].addr = seesaw->client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 2;
	msgs[1].buf = buf; 

	ret = i2c_transfer(seesaw->client->adapter, msgs, 2);
	pr_info("ret: %d\n",ret);
	return (ret == 2) ? 0 : ret;*/
}

/* Read the soil temperature for the seesaw soil sesnor */
static int seesaw_read_temperature(struct seesaw_dev *seesaw, int *temp)
{
	u8 buf[4];
	int ret;
	s32 raw_temp;

	mutex_lock(&seesaw->lock);
	ret = seesaw_read_reg(seesaw, SEESAW_STATUS_BASE, SEESAW_STATUS_TEMP,
				buf, sizeof(buf));
	mutex_unlock(&seesaw->lock);
	if (ret < 0)
		return ret;

	raw_temp = ((buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3]);
	*temp = (raw_temp * 100) >> 16; /* Convert to centidegrees */
	return 0;
} 

static int seesaw_read_moisture(struct seesaw_dev *seesaw, int *moisture)
{
	u8 buf[2];
	int ret;

	mutex_lock(&seesaw->lock);
	ret = seesaw_read_reg(seesaw, SEESAW_STATUS_BASE, SEESAW_TOUCH_CHANNEL,	
				buf, sizeof(buf));
	mutex_unlock(&seesaw->lock);

	if (ret < 0)
		return ret;
	
	*moisture = (buf[0] << 8 | buf[1]);
	return 0;
}

/* User reading data from /dev/stemma */
static ssize_t seesaw_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	pr_info("stemma_fops reading function\n");

	int moisture, temperature, ret, len;
	char buf[64];
	char kbuf[64];
	struct seesaw_dev *seesaw; 

	seesaw = container_of(file->private_data, struct seesaw_dev, seesaw_miscdevice);
	
	/* Write to the measurement trigger register
	ret = i2c_smbus_write_byte_data(seesaw->client, STEMMA_MEASURE_CMD, 0x01); */

	ret = seesaw_read_moisture(seesaw, &moisture);
	if (ret < 0){
		pr_info("Failed to trigger measurement, error: %d\n", ret);

		switch(ret){
			case -ENXIO:
				dev_err(seesaw->dev, "No such I2C device\n");
				break;
			case -EAGAIN:	
				dev_err(seesaw->dev, "resource temporarily unavailable -bus busy?\n");
				break;
			case -ETIMEDOUT:
				dev_err(seesaw->dev, "I2C transaction timed out\n");
				break;
			case -EIO:
				dev_err(seesaw->dev, "I/O Error - no ACK recevied?\n");
				break;
			default:
				dev_err(seesaw->dev, "Unknown error\n");
		}
	}

	ret = seesaw_read_temperature(seesaw, &temperature);
	if (ret < 0)
		return ret;

	len = snprintf(kbuf, sizeof(kbuf), "moisture: %d\ntemperature: %d\n", moisture, temperature);

	if (count < len)
		return -EINVAL;
	
	/* wait for measurement to complete*/
	msleep(SEESAW_READING_DELAY);

	/* Read moisture data 
	moisture  = i2c_smbus_read_byte_data(seesaw->client, STEMMA_TOUCH_REG);
	if (moisture  < 0){
		pr_info("Failed to read moisture data\n");
		return -EFAULT;
	}*/

	/* Read temperature data 
	temperature = i2c_smbus_read_word_data(seesaw->client, STEMMA_TEMP_REG);
	if ( temperature < 0){
		pr_info("Failed tor ead temperature data\n");
		return -EFAULT;
	}
	temperature = ((temperature * 200) / 65536) - 50;
	

	size = sprintf(buf,"Moisture: %02x, Temperature: %02x", moisture, temperature);*/

	/*
	 * Replace NULL by \n. It is not needed to have a char array
	 * ended with \0 character.
	 */
	buf[len] = '\n';

	/* send size+1 to include the \n character */
	if (*ppos == 0){
		/*if (copy_to_user(userbuf, buf, size+1)){*/
		if (copy_to_user(userbuf, kbuf, strlen(kbuf))){
			pr_info("Failed to return led_value to user space\n");
			return -EFAULT;
		}
		*ppos+=1;
		return len+1;
	}
	
	return 0;
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

	/* Allocate the stemma private structure */
	seesaw = devm_kzalloc(&client->dev, sizeof(struct seesaw_dev), GFP_KERNEL);
	
	/* Store pointer tot he device-structure in the bus device context */
	i2c_set_clientdata(client, seesaw);

	/* Store pointer to I2C client */
	seesaw->client = client;

	/*
	 * Initialize the misc device, stemma is incremented 
	 * after each probe call
	 */
	sprintf(seesaw->name, "stemma%02d", counter++);
	dev_info(&client->dev, "stemma_probe is entered on %s\n", seesaw->name);

	seesaw->seesaw_miscdevice.name = seesaw->name;
	seesaw->seesaw_miscdevice.minor = MISC_DYNAMIC_MINOR; 
	seesaw->seesaw_miscdevice.fops = &seesaw_fops;

	/* Register misc device */
	return misc_register(&seesaw->seesaw_miscdevice);
	
	dev_info(&client->dev, "seesaw_probe is exited on %s\n", seesaw->name);
	return 0;
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
