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

static int seesaw_read_raw(struct iio_dev *iio,
			struct iio_chan_spec const *channel, int *val1,
			int *val2, long mask)
{
	pr_info("Conduction seesaw read raw function\n");

	return 0;
}

static const struct iio_chan_spec seesaw_channel[] = {
	{
		.type = IIO_TEMP,
		.indexed = 0,
		.channel = 0,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | \
				BIT(IIO_CHAN_INFO_SCALE),
	},
};

/* Information about the device */
static const struct iio_info seesaw_info = {
	.read_raw = seesaw_read_raw,
	/*.attrs = &seesaw_attribute_group,*/
};

static int seesaw_probe(struct i2c_client *client)
{
	struct iio_dev *indio_dev;
	struct seesaw_dev *seesaw;
	
	/* Allocate memory for the IIO device */
	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(seesaw));
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
