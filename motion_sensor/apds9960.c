#include <linux/module.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#define APDS9960_MAJOR 42
#define APDS9960_MAX_MINORS 1
#define APDS9960_ADDR 0x39
#define PROX_DATA_REG 0x9C
#define DEVICE_NAME "apds9960"

#define APDS9960_ENABLE 0x80
#define APDS9960_PDATA 0x90 // prox data
#define APDS9960_PILT 0x89
#define APDS9960_PIHT 0x8B
#define APDS9960_STATUS 0x92
#define APDS9960_WAIT_TIME 0x83
#define APDS9960_ADC_TIME 0x82
#define APDS9960_CONTROL 0x8f
#define APDS9960_PERS 0x8c
#define APDS9960_CONFIG_THREE 0x9f

#define APDS9960_GCONF4_REG 0xAB
#define APDS9960_GSTATUS_REG 0xAF
#define APDS9960_GFIFO_U_REG 0xFC

#define APDS9960_STATUS_GINT (1<<2)

// TODO Have this be configurable
#define APDS9960_INT_PIN 23

/*
 * Color data is reported using two bytes, one
 * register for the low order bits, the next for
 * the high order bits.
 *
 * Reading low order bits latches the high order bits
 * until the next read, preventing data corruption. The
 * same is done for the clear channel, which will latch
 * all other channels. This means that any read to clear
 * must also be a read of all channels, and any read to
 * one channel must be a complete read. Otherwise, this
 * will lock-up the sensor.
 */
// clear data
#define APDS9960_CDATAL 0x94 // low byte
#define APDS9960_CDATAH 0x95 // high byte
// read data
#define APDS9960_RDATAL 0x96
#define APDS9960_RDATAH 0x97
// green data
#define APDS9960_GDATAL 0x98
#define APDS9960_GDATAH 0x99
// blue data
#define APDS9960_BDATAL 0x9A
#define APDS9960_BDATAH 0x9B

// Enable bitfields 
#define APDS9960_ON_ENABLE (1)
#define APDS9960_ALS_ENABLE (1<<1)
#define APDS9960_PROX_ENABLE (1<<2)
#define APDS9960_WAIT_ENABLE (1<<3)
#define APDS9960_ALS_INT_ENABLE (1<<4)
#define APDS9960_PROX_INT_ENABLE (1<<5)
#define APDS9960_GESTURE_ENABLE (1<<6)

struct apds9960_dev {
  struct i2c_client* client;
  struct miscdevice apds9960_miscdevice;
  struct input_dev *input;
  struct gpio_desc *gpio;
  int irq;
  wait_queue_head_t wq;
  bool color_ready;
  struct timer_list timer;
  char name[8]; /* apds9960 */
  struct work_struct gesture_work; // Add workqueue for bottom half
};

static const struct of_device_id apds9960_dt_ids[] = {
  { .compatible = "arrow,apds", },
  { }
};
MODULE_DEVICE_TABLE(of, apds9960_dt_ids);

static const struct i2c_device_id i2c_ids[] = {
  { .name = "apds", },
  { }
};
MODULE_DEVICE_TABLE(i2c, i2c_ids);

/* Timer callback that marks data as ready and wakes waiting processes */
static void sensor_timer_cb(struct timer_list *t)
{
    struct apds9960_dev *dev = from_timer(dev, t, timer);
    dev->color_ready = true;
    wake_up(&dev->wq);
}

static ssize_t apds9960_write_file(struct file *file, const char __user *userbuf,
    size_t count, loff_t *ppos)
{
  int ret;
  unsigned long val;
  char buf[5];
  struct apds9960_dev * apds9960;
  apds9960 = container_of(file->private_data, struct apds9960_dev, apds9960_miscdevice);
  copy_from_user(buf, userbuf, count);
  /* Convert char array to char string */
  buf[count-1] = '\0';
  /* Convert the string to an unsigned long */
  ret = kstrtoul(buf, 0, &val);

  i2c_smbus_write_byte_data(apds9960->client, APDS9960_ENABLE, val);
  return count;
}

// Sets gain controls. Bit range 1:0 (leftshift 0) light gain 
// 3:2 (leftshift 2) is for proximity gain
// 7:6 (leftshift 6) is for led gain
static void apds9960_set_config(struct apds9960_dev* apds9960, int value)
{
  i2c_smbus_write_byte_data(apds9960->client, APDS9960_CONTROL, value);
}

static void apds9960_set_enable(struct apds9960_dev* apds9960, int value)
{
  i2c_smbus_write_byte_data(apds9960->client, APDS9960_ENABLE, value);
}

static void apds9960_set_adc_time(struct apds9960_dev* apds9960, int value)
{
  i2c_smbus_write_byte_data(apds9960->client, APDS9960_ADC_TIME, value);
}

static void apds9960_set_prox_pers(struct apds9960_dev* apds9960, u8 value)
{
  i2c_smbus_write_byte_data(apds9960->client, APDS9960_PERS, value << 4);
}

static void apds9960_set_sleep_after_interrupt(struct apds9960_dev* apds9960, u8 value)
{
  i2c_smbus_write_byte_data(apds9960->client, APDS9960_CONFIG_THREE, (value & 1) << 4);
}

// Returns 0 on success, and sets the outparams to the color values
static int apds9960_read_colors_crgb(struct apds9960_dev* apds9960, u16*C, u16* R, u16* G, u16* B)
{
  int avalid;
  avalid = i2c_smbus_read_byte_data(apds9960->client, APDS9960_STATUS) & 1;
  if(avalid) {
    pr_info("Reading color data\n");
    // Read RGB values
    *C = i2c_smbus_read_byte_data(apds9960->client, APDS9960_CDATAL);
    *C |= i2c_smbus_read_byte_data(apds9960->client, APDS9960_CDATAH) << 8;
    *R = i2c_smbus_read_byte_data(apds9960->client, APDS9960_RDATAL);
    *R |= i2c_smbus_read_byte_data(apds9960->client, APDS9960_RDATAH) << 8;
    *G = i2c_smbus_read_byte_data(apds9960->client, APDS9960_GDATAL);
    *G |= i2c_smbus_read_byte_data(apds9960->client, APDS9960_GDATAH) << 8;
    *B = i2c_smbus_read_byte_data(apds9960->client, APDS9960_BDATAL);
    *B |= i2c_smbus_read_byte_data(apds9960->client, APDS9960_BDATAH) << 8;
  } else {
    pr_info("Color not valid, skipping!!!!");
    return avalid;
  }
  return 0;
}

int apds9960_read_proximity(struct apds9960_dev* apds9960)
{
  int pvalid, expval;
  pvalid = i2c_smbus_read_byte_data(apds9960->client, APDS9960_STATUS) & (1<<1);
  if(pvalid) {
    pr_info("Reading prox data\n");
    expval = i2c_smbus_read_byte_data(apds9960->client, PROX_DATA_REG);
    if (expval < 0) {
        return -1;
    }
  } else {
    return -1;
  }
  return expval;
}

/* User is reading data from /dev/apds9960XX */
static ssize_t apds9960_read_file(struct file *file, char __user *userbuf,
    size_t count, loff_t *ppos)
{
  int proximity, size, ret, avalid;
  u16 C, R, G, B;
  char buf[60]; // Increase buffer size for RGB values
  struct apds9960_dev * apds9960;
  apds9960 = container_of(file->private_data, struct apds9960_dev, apds9960_miscdevice);
  dev_info(&apds9960->client->dev, "Enter Read");

  apds9960_set_adc_time(apds9960, 0xff);
  apds9960_set_config(apds9960, 3 | 2 << 2 | 0 << 6);

  apds9960_set_enable(apds9960, APDS9960_ON_ENABLE | APDS9960_PROX_ENABLE | APDS9960_ALS_ENABLE);
  apds9960_set_sleep_after_interrupt(apds9960, 0);
  proximity = apds9960_read_proximity(apds9960);
  if(proximity < 0) {
    pr_info("Prox not valid, skipping!!!!");
  }
  dev_info(&apds9960->client->dev, "Prox read");

  apds9960->color_ready = false;
  mod_timer(&apds9960->timer, msecs_to_jiffies(30));
  ret = wait_event_interruptible(apds9960->wq, apds9960->color_ready);
  if(ret)
    return ret;

  avalid = apds9960_read_colors_crgb(apds9960, &C, &R, &G, &B);
  dev_info(&apds9960->client->dev, "Color read");

  // Test if both failed
  if(!avalid && (proximity < 0)) {
      pr_info("Data not ready!\n");
      return -EFAULT;
  }

  apds9960_set_enable(apds9960, APDS9960_ON_ENABLE | APDS9960_PROX_ENABLE | APDS9960_PROX_INT_ENABLE);
  // Setup interrupt thresholds just for testing purposes
  apds9960_set_prox_pers(apds9960, 1);
  i2c_smbus_write_byte_data(apds9960->client, APDS9960_PILT, 0x10);
  i2c_smbus_write_byte_data(apds9960->client, APDS9960_PIHT, 0xa0);

  // Prepare the output buffer
  size = sprintf(buf, "Prox: %02x, CRGB: C(%04x) R(%04x) G(%04x) B(%04x)",
                 proximity, C, R, G, B);
  /*
   * Replace NULL by \n. It is not needed to have a char array
   * ended with \0 character.
   */
  buf[size] = '\n';
  /* Send size+1 to include the \n character */
  if(*ppos == 0) {
    if(copy_to_user(userbuf, buf, size+1)) {
      pr_info("Failed to return value to user space\n");
      return -EFAULT;
    }
    *ppos+=1;
    return size+1;
  }
  return 0;
}

static const struct file_operations apds9960_fops = {
  .owner = THIS_MODULE,
  .read = apds9960_read_file,
  .write = apds9960_write_file,
};
static void gesture_work_handler(struct work_struct *work)
{
  struct apds9960_dev *apds9960 = container_of(work, struct apds9960_dev, gesture_work);
  struct i2c_client *client = apds9960->client;
  u8 status, gesture_data;
  int i;
  
  // Read status register
  status = i2c_smbus_read_byte_data(client, APDS9960_STATUS);
  
  if (status & APDS9960_STATUS_GINT) {
    // Process gesture data from FIFO
    for (i = 0; i < 4; i++) {
      gesture_data = i2c_smbus_read_byte_data(client, APDS9960_GFIFO_U_REG + i);
      
      switch (gesture_data) {
        case 0x01: // Up gesture
          input_report_key(apds9960->input, KEY_UP, 1);
          input_sync(apds9960->input);
          input_report_key(apds9960->input, KEY_UP, 0);
          input_sync(apds9960->input);
          break;
        // Add other gesture cases here
      }
    }
  }
}

static irqreturn_t apds9960_isr(int irq, void *data)
{
  struct apds9960_dev *apds9960 = data;
  
  // Schedule bottom half immediately
  schedule_work(&apds9960->gesture_work);
  
  return IRQ_HANDLED;
}

static int apds9960_probe (struct i2c_client * client)
{
  static int counter = 0;
  struct platform_device *pdev;
  struct apds9960_dev * apds9960;
  int err;
  if (!client->dev.of_node) {
      dev_err(&client->dev, "No device tree node found\n");
      return -ENODEV;
  }
  dev_info(&client->dev, "Device tree node: %pOF\n", client->dev.of_node);

  /* Allocate the private structure */
  apds9960 = devm_kzalloc(&client->dev, sizeof(struct apds9960_dev), GFP_KERNEL);
  pdev = to_platform_device(&client->dev);
  /* Store pointer to the device-structure in bus device context */
  i2c_set_clientdata(client,apds9960);
  /* Store pointer to I2C client */
  apds9960->client = client;

  // Allocate input device
  apds9960->input = devm_input_allocate_device(&client->dev);
  if (!apds9960->input) {
      dev_err(&client->dev, "Failed to allocate input device");
      return -ENOMEM;
  }

  // Set input device name/ID
  apds9960->input->name = "APDS-9960 Gesture Sensor";
  apds9960->input->phys = "i2c::gesture";
  apds9960->input->id.bustype = BUS_I2C;

  // Register event types (KEY for gestures)
  __set_bit(EV_KEY, apds9960->input->evbit);

  // Define gesture keys (up/down/left/right)
  input_set_capability(apds9960->input, EV_KEY, KEY_UP);
  input_set_capability(apds9960->input, EV_KEY, KEY_DOWN);
  input_set_capability(apds9960->input, EV_KEY, KEY_LEFT);
  input_set_capability(apds9960->input, EV_KEY, KEY_RIGHT);

  err = input_register_device(apds9960->input);
  if (err) {
      dev_err(&client->dev, "Failed to register input device");
      return err;
  }

  // Power on and enable gesture mode (see datasheet registers)
  i2c_smbus_write_byte_data(client, APDS9960_ENABLE, 
                           APDS9960_ON_ENABLE | APDS9960_GESTURE_ENABLE );

  // Set gesture thresholds (adjust based on testing)
  // GESTURE_RIGHT_OFFSET_REGISTER, w/ gpulse on bits 5:0
  i2c_smbus_write_byte_data(client, 0xA9, 0x89); // 16 pulses, 32 us
  i2c_smbus_write_byte_data(client, APDS9960_GCONF4_REG, 0x01); // 4 gesture events

  /* Wait mechanism */
  init_waitqueue_head(&apds9960->wq);
  timer_setup(&apds9960->timer, sensor_timer_cb, 0);

  /* Get GPIO descriptor and IRQ from device tree */
  // Use gpio driver to setup pin
  apds9960->gpio = gpiod_get(&client->dev, "apdsint", GPIOD_IN);
  if (IS_ERR(apds9960->gpio)) {
    if(PTR_ERR(apds9960->gpio) == -ENOENT) {
      dev_err(&client->dev, "GPIO get failed, ENOENT\n");
    }
    dev_err(&client->dev, "GPIO get failed: %ld\n", PTR_ERR(apds9960->gpio));
    return PTR_ERR(apds9960->gpio);
  }

  apds9960->irq = gpiod_to_irq(apds9960->gpio);
  if (apds9960->irq < 0) {
    dev_err(&client->dev, "Failed to get IRQ: %d\n", apds9960->irq);
    return apds9960->irq;
  }

  dev_info(&client->dev, "Using IRQ: %d\n", apds9960->irq);

  err = devm_request_irq(&client->dev, apds9960->irq, apds9960_isr, IRQF_TRIGGER_FALLING, DEVICE_NAME, apds9960);
  if(err) {
    dev_info(&client->dev, "ERM, what the sigma!?: %d\n", err);
    return -ENOENT;
  }

  /*
   * Initialize the misc device, apds9960 is incremented
   * after each probe call
   */
  sprintf(apds9960->name, "apds9960%02d", counter++);
  apds9960->apds9960_miscdevice.name = apds9960->name;
  apds9960->apds9960_miscdevice.minor = MISC_DYNAMIC_MINOR;
  apds9960->apds9960_miscdevice.fops = &apds9960_fops;

  /* Register the misc device */
  dev_info(&client->dev, "apds9960 probe successful");
  return misc_register(&apds9960->apds9960_miscdevice);
  return 0;
}

void apds9960_remove(struct i2c_client * client)
{
  struct apds9960_dev * apds9960;
  /* Get device structure from bus device context */
  apds9960 = i2c_get_clientdata(client);
  dev_info(&client->dev,
      "apds9960_remove is entered on %s\n", apds9960->name);
  /* Deregister misc device */
  misc_deregister(&apds9960->apds9960_miscdevice);
  devm_free_irq(&client->dev, apds9960->irq, apds9960);
  gpiod_put(apds9960->gpio);
  del_timer(&apds9960->timer);
  dev_info(&client->dev,
      "apds9960_remove is exited on %s\n", apds9960->name);
}


static struct i2c_driver apds9960_driver = {
  .driver = {
    .name = "apds9960",
    .owner = THIS_MODULE,
    .of_match_table = apds9960_dt_ids,
  },
  .probe = apds9960_probe,
  .remove = apds9960_remove,
  .id_table = i2c_ids,
};

module_i2c_driver(apds9960_driver);

MODULE_AUTHOR("Christopher Odom");
MODULE_DESCRIPTION("APDS-9960 proximity sensor char device driver");
MODULE_LICENSE("GPL");
