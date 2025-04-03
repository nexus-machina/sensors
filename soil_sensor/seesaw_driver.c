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
#include <linux/i2c.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
/*#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/of.h>*/

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

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
	/*struct device *dev;*/
	struct mutex lock;
	char name[8]; /* stemmaXX */
};

static int seesaw_read(struct seesaw_dev *seesaw, int *val)
{
	struct i2c_client *client = seesaw->client;
	u8 buf[2];
	buf[0] = (0x10 >> 8) & 0xFF; //high byte
	buf[1] = 0x0F & 0xFF;
	int ret;

	
	/*mutex_lock(&seesaw->lock);*/
	ret = i2c_master_send(client, 0x0F, 1);
	if (ret < 0)
		pr_info("I2C write failed\n");
	else
		pr_info("I2C write successful\n");

	msleep(20);
	if (ret < 0)
		pr_info("Failed to set base register: %d\n", ret);
	for (int i = 0; i < 10; i++){
		msleep(100);
		ret = i2c_smbus_read_word_data(client, 0x10);
		/*ret = i2c_master_recv(client, buf, 1);*/
		if (ret >= 0){
			pr_info("i2c_master_recv success: %d\n", ret);
		}
	}
	if (ret < 0){
		pr_info("i2c_master_recv failed: %d\n", ret);
		return ret;
	}

	/*mutex_unlock(&seesaw->lock);*/

	return ret;
}

static int seesaw_read_raw(struct iio_dev *iio_dev,
			struct iio_chan_spec const *channel, int *val1,
			int *val2, long mask)
{
	struct seesaw_dev *seesaw = iio_priv(iio_dev);
	int ret; 

	pr_info("Conducting seesaw read raw function\n");

	if (mask == IIO_CHAN_INFO_RAW){
		pr_info("IIO_CHAN_INFO_RAW\n");
		ret = seesaw_read(seesaw, val1);
		if (ret < 0)
			return ret;
	}else
		pr_info("Somethign different");

	return IIO_VAL_INT;
}

static ssize_t seesaw_show_sample_freqs(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"Callback for seesaw attribute\n");
}

static IIO_DEVICE_ATTR(sampling_frequency_available, S_IRUGO,
		seesaw_show_sample_freqs, NULL, 0);

/* An arrary of pointers to the attributes */
static struct attribute *seesaw_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	NULL, 
};

/* Seesaw attribute group */
static const struct attribute_group seesaw_attribute_group = {
	.name = "seesaw_group",
	.attrs = seesaw_attributes,
};

static const struct iio_chan_spec seesaw_channel[] = {
	{
		/*.type = IIO_TEMP,*/
		.type = IIO_CAPACITANCE,
		/*.type = IIO_VOLTAGE,*/
		.indexed = 1,
		.channel = 0,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
	},
};

/* Information about the device */
static const struct iio_info seesaw_info = {
	.read_raw = seesaw_read_raw,
	.attrs = &seesaw_attribute_group,
};

static int seesaw_probe(struct i2c_client *client)
{
	struct iio_dev *indio_dev;
	struct seesaw_dev *seesaw;
	int ret;

	pr_info("Execution starts here\n");
	
	/* Allocate memory for the IIO device */
	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*seesaw));
	if (!indio_dev)
		return -ENOMEM;

	/* Initialize the IIO device fields (device name, ADC device channels, etc) */
	seesaw = iio_priv(indio_dev);
	seesaw->client = client;
	indio_dev->name = dev_name(&client->dev); 
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &seesaw_info;

	indio_dev->channels = seesaw_channel;
	indio_dev->num_channels = ARRAY_SIZE(seesaw_channel);

	/* Register the device with the IIO core */
	devm_iio_device_register(&client->dev,indio_dev);

	return 0;
}


/* list of devices supported by the driver */
static const struct of_device_id seesaw_dt_ids[] = {
	{ .compatible = "adafruit,seesaw", },
	{}
};
MODULE_DEVICE_TABLE(of, seesaw_dt_ids);

/* an array of i2c_device_id structures */
static const struct i2c_device_id i2c_ids[] = {
	{ .name = "seesaw", },
	{ }
};
MODULE_DEVICE_TABLE(i2c, i2c_ids);

/* i2c_driver structure that will be registered to the I2C bus */
static struct i2c_driver seesaw_driver = {
	.probe	= 	seesaw_probe,
	.id_table =	i2c_ids,
	.driver = {
		.name	=	"seesaw",
		.owner	=	THIS_MODULE,	
		.of_match_table = seesaw_dt_ids
	},
};
/* register the driver with the I2C bus */
module_i2c_driver(seesaw_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Erastus Toe <chukpozohnt@gmil.com>");
MODULE_DESCRIPTION("This driver controls the Adafruit STEMMA Soil Sensor");
