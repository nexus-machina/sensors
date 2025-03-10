#include "apds9960.h"

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

  dev_info(&apds9960->client->dev, "TODO: Write commands, instead here's a state report:");
  u8 gstatus = i2c_smbus_read_byte_data(apds9960->client, APDS9960_GSTATUS_REG);
  bool gfov = false, gvalid = false;
  if(gstatus & APDS9960_GSTATUS_GFOV) {
    gfov = true;
  }
  if(gstatus & APDS9960_GSTATUS_GVALID) {
    gvalid = true;
  }
  dev_info(&apds9960->client->dev, "GSTATUS { gfov: %d, gvalid: %d }", gfov, gvalid);

  u8 status = i2c_smbus_read_byte_data(apds9960->client, APDS9960_STATUS);
  bool cpsat = status & APDS9960_STATUS_CPSAT;
  bool pgsat = status & APDS9960_STATUS_PGSTAT;
  bool pint = status & APDS9960_STATUS_PINT;
  bool aint = status & APDS9960_STATUS_AINT;
  bool gint = status & APDS9960_STATUS_GINT;
  bool pvalid = status & APDS9960_STATUS_PVALID;
  bool avalid = status & APDS9960_STATUS_AVALID;
  dev_info(&apds9960->client->dev, "GSTATUS { cpsat: %d pgsat: %d pint: %d aint: %d gint: %d pvalid: %d avalid: %d }", cpsat, pgsat, pint, aint, gint, pvalid, avalid);

  u8 gconf = i2c_smbus_read_byte_data(apds9960->client, APDS9960_GCONF4_REG);
  bool gmode = gconf & APDS9960_GCONF4_GMODE;
  bool gien = gconf & APDS9960_GCONF4_GIEN;
  dev_info(&apds9960->client->dev, "GSTATUS { gien: %d, gmode: %d }", gien, gmode);

  return count;
}

// Sets gain controls. Bit range 1:0 (leftshift 0) light gain 
void apds9960_set_control_1(struct apds9960_dev* apds9960, struct apds9960_ctrl_1_cfg cfg)
{
  u8 write_value = cfg.ldrive << 6 | cfg.pgain << 2 | cfg.again;
  i2c_smbus_write_byte_data(apds9960->client, APDS9960_CONTROL_1, write_value);
}

void apds9960_set_gconf_1(struct apds9960_dev* apds9960, struct apds9960_gconf_1_cfg cfg)
{
  u8 write_value = cfg.gfifoth << 6 | cfg.gexmsk << 2 | cfg.gexpers; 
  i2c_smbus_write_byte_data(apds9960->client, APDS9960_GCONF1_REG, write_value);
}

void apds9960_set_gconf_2(struct apds9960_dev* apds9960, struct apds9960_gconf_2_cfg cfg)
{
  u8 write_value = cfg.ggain << 5 | cfg.gldrive << 3 | cfg.gwtime; 
  i2c_smbus_write_byte_data(apds9960->client, APDS9960_GCONF2_REG, write_value);
}

void apds9960_set_gpulse(struct apds9960_dev* apds9960, struct apds9960_gpulse_cfg cfg)
{
  u8 write_value = cfg.gplen << 6 | cfg.gpulse;
  i2c_smbus_write_byte_data(apds9960->client, APDS9960_GCONF2_REG, write_value);
}

static void apds9960_set_enable(struct apds9960_dev* apds9960, int value)
{
  i2c_smbus_write_byte_data(apds9960->client, APDS9960_ENABLE, value);
}

static void apds9960_set_adc_time(struct apds9960_dev* apds9960, int value)
{
  i2c_smbus_write_byte_data(apds9960->client, APDS9960_ADC_TIME, value);
}

// Valid since the remaining bits are reserved
static void apds9960_set_prox_pers(struct apds9960_dev* apds9960, u8 value)
{
  i2c_smbus_write_byte_data(apds9960->client, APDS9960_PERS, value << 4);
}

static void apds9960_set_config_three(struct apds9960_dev* apds9960, u8 value)
{
  i2c_smbus_write_byte_data(apds9960->client, APDS9960_CONFIG_THREE, value);
}

void apds9960_non_gest_clear(struct apds9960_dev* apds9960)
{
  i2c_smbus_write_byte_data(apds9960->client, APDS9960_AICLEAR, 0);
}

void apds9960_assert_pon(struct apds9960_dev* apds9960)
{
  apds9960_set_enable(apds9960, APDS9960_ENABLE_ON);
}

void apds9960_idle_assert_gesture(struct apds9960_dev* apds9960)
{
  // GESTURE_RIGHT_OFFSET_REGISTER, w/ gpulse on bits 5:0
  // i2c_smbus_write_byte_data(apds9960->client, APDS9960_GOFFSET_R, 0x89); // 16 pulses, 32 us
  // Run forever!
  apds9960_set_gconf_1(apds9960, (struct apds9960_gconf_1_cfg) {.gfifoth = 1, .gexmsk = 0xf, .gexpers = 3});
  apds9960_set_gconf_2(apds9960, (struct apds9960_gconf_2_cfg) {.ggain = 1, .gldrive = 0, .gwtime = 0});
  apds9960_set_gpulse(apds9960, (struct apds9960_gpulse_cfg)  {.gplen = 3, .gpulse = 16});

  i2c_smbus_write_byte_data(apds9960->client, APDS9960_GEXTH, 0); // Exit threshold, we run indefinitely
  apds9960_set_config_three(apds9960, APDS9960_CONFIG_PCMP_ENABLE);
  i2c_smbus_write_byte_data(apds9960->client, APDS9960_GCONF4_REG, APDS9960_GCONF4_GIEN | APDS9960_GCONF4_GMODE);
  // Power on and enable gesture mode (see datasheet registers) apds9960->state = APDS9960_STATE_MOTION;
  apds9960_set_enable(apds9960, APDS9960_ENABLE_ON | APDS9960_ENABLE_GESTURE);
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
  apds9960_set_control_1(apds9960, (struct apds9960_ctrl_1_cfg) {.ldrive = 1, .pgain = 2, .again = 0});

  apds9960_set_config_three(apds9960, APDS9960_CONFIG_PCMP_ENABLE);
  apds9960_set_enable(apds9960, APDS9960_ENABLE_ON | APDS9960_ENABLE_PROX | APDS9960_ENABLE_ALS);

  apds9960->data_ready = false;
  apds9960->pvalid = false;
  apds9960->avalid = false;

  ret = wait_event_interruptible(apds9960->wq, &apds9960->data_ready);
  if(ret) {
    return ret;
  }

  proximity = apds9960_read_proximity(apds9960);
  if(proximity < 0) {
    pr_info("Prox not valid, skipping!!!!");
  }
  dev_info(&apds9960->client->dev, "Prox read");
  avalid = apds9960_read_colors_crgb(apds9960, &C, &R, &G, &B);
  dev_info(&apds9960->client->dev, "Color read");

  // Test if both failed
  if(!avalid && (proximity < 0)) {
      pr_info("Data not ready!\n");
      return -EFAULT;
  }

  apds9960_set_enable(apds9960, APDS9960_ENABLE_ON | APDS9960_ENABLE_PROX | APDS9960_ENABLE_PROX_INT);
  // Setup interrupt thresholds just for testing purposes
  // apds9960_set_prox_pers(apds9960, 1);
  // i2c_smbus_write_byte_data(apds9960->client, APDS9960_PILT, 0x10);
  // i2c_smbus_write_byte_data(apds9960->client, APDS9960_PIHT, 0xa0);

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
    u8 gflvl = i2c_smbus_read_byte_data(client, APDS9960_FIFO_LEVEL);
    u8 gstatus = i2c_smbus_read_byte_data(client, APDS9960_GSTATUS_REG);
    if(gstatus & APDS9960_GSTATUS_GFOV)
    {
      dev_err(&client->dev, "Overflow event detected!!!");
    }
    while(gflvl) {
      // Process gesture data from each FIFO queue
      for (i = 0; i < 4; i++) {
        gesture_data = i2c_smbus_read_byte_data(client, APDS9960_GFIFO_U_REG + i);
        // TODO This right here needs to run a basic gesture detection algorithm by looking
        // at the values reported by each channel (UDLR)
        //
        // Make sure it's simple, then report to linux input system

        // switch (gesture_data) {
        //   default: // Up gesture
        //     input_report_key(apds9960->input, KEY_DOWN, 1);
        //     input_sync(apds9960->input);
        //     input_report_key(apds9960->input, KEY_UP, 0);
        //     input_sync(apds9960->input);
        //     break;
        //   // Add other gesture cases here
        // }
      }
      gflvl = i2c_smbus_read_byte_data(client, APDS9960_FIFO_LEVEL);
    }
    // assert gmode -> continue gesture data collection
    i2c_smbus_write_byte_data(apds9960->client, APDS9960_GCONF4_REG, APDS9960_GCONF4_GIEN | APDS9960_GCONF4_GMODE);
  } else if (status & APDS9960_STATUS_AVALID || status & APDS9960_STATUS_PVALID) {
    if (status & APDS9960_STATUS_AVALID)
    {
      dev_info(&client->dev, "AVALID");
      apds9960->avalid = true;
    }
    if (status & APDS9960_STATUS_PVALID)
    {
      dev_info(&client->dev, "PVALID");
      apds9960->pvalid = true;
    }
    apds9960->data_ready = true;
    wake_up(&apds9960->wq);
    apds9960_non_gest_clear(apds9960);
    apds9960_set_enable(apds9960, APDS9960_ENABLE_ON);
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

  /* Wait mechanism */
  init_waitqueue_head(&apds9960->wq);

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
    dev_info(&client->dev, "IRQ Request Err: %d\n", err);
    return -ENOENT;
  }

  INIT_WORK(&apds9960->gesture_work, gesture_work_handler);

  /*
   * Initialize the misc device, apds9960 is incremented
   * after each probe call
   */
  sprintf(apds9960->name, "apds9960%02d", counter++);
  apds9960->apds9960_miscdevice.name = apds9960->name;
  apds9960->apds9960_miscdevice.minor = MISC_DYNAMIC_MINOR;
  apds9960->apds9960_miscdevice.fops = &apds9960_fops;

  apds9960_assert_pon(apds9960);

  // Configure gain controls now so that the gesture engine can make use of them
  apds9960_set_adc_time(apds9960, 0xff);
  apds9960_set_control_1(apds9960, (struct apds9960_ctrl_1_cfg) {.ldrive = 1, .pgain = 2, .again = 1});
  apds9960_set_config_three(apds9960, APDS9960_CONFIG_PCMP_ENABLE);

  apds9960_set_enable(apds9960, APDS9960_ENABLE_ON);
  apds9960_idle_assert_gesture(apds9960);

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
  /* Go to sleep... */
  apds9960_set_enable(apds9960, 0);
  dev_info(&client->dev,
      "apds9960_remove is entered on %s\n", apds9960->name);
  /* Deregister misc device */
  misc_deregister(&apds9960->apds9960_miscdevice);
  devm_free_irq(&client->dev, apds9960->irq, apds9960);
  gpiod_put(apds9960->gpio);
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
