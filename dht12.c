#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <asm/mach/map.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/types.h>

#define DRV_VERSION "V2.0"

typedef struct dht12_data_format{
        unsigned char val;
        unsigned char val2;
}data_format;
struct dht12 {
        struct device                   *dev;
	struct i2c_client		*client;
        /* The iio sysfs interface doesn't prevent concurrent reads: */
        struct mutex                    lock;
        data_format                     temperature;
        data_format                     humidity;
};
static struct i2c_client *dht12_client;
static struct mutex my_mutex;
/*
static int i2c_dht12_read_len(struct i2c_client *client,unsigned char reg_addr,unsigned char len,unsigned char *buf)
{
	int ret;
        unsigned char txbuf = reg_addr;
        struct i2c_msg msg[] = {
                {client->addr,0,1,&txbuf},
                {client->addr,1,len,buf}
        };
        ret = i2c_transfer(client->adapter,msg,2);
        if(ret < 0) {
                printk("i2c_transfer read len error\n");
                return -1;
        }
        return 0;
}
static int i2c_dht12_read_byte(struct i2c_client *client,unsigned char reg_addr)
{
	int ret;
	unsigned char txbuf = reg_addr;
	unsigned char rxbuf;
	struct i2c_msg msg[] = {
		{client->addr,0,1,&txbuf},
		{client->addr,I2C_M_RD,1,&rxbuf}
	};
	ret = i2c_transfer(client->adapter,msg,2);
	if(ret < 0) {
		printk("i2c_transfer read error\n");
                return -1;
	}
	return rxbuf;
}

static int i2c_dht12_write_byte(struct i2c_client *client,unsigned char reg_addr,unsigned char data_buf)
{
        int ret;
        unsigned char txbuf[] = {reg_addr,data_buf};
        struct i2c_msg msg[] = {client->addr,0,2,txbuf};
        ret = i2c_transfer(client->adapter,msg,1);
        if(ret < 0) {
                printk("i2c_transfer write error\n");
                return -1;
        }
        return 0;
}
*/

static int dht12_read_measurement(struct dht12 *data, bool temp) {
	char ret;
	int i;
	unsigned char data_buf[5],checksum;
	struct i2c_client *client = data->client;
	ret = 0;
	data_buf[0] = i2c_smbus_read_byte_data(client,0x00);
	data_buf[1] = i2c_smbus_read_byte_data(client,0x01);
	data_buf[2] = i2c_smbus_read_byte_data(client,0x02);
	data_buf[3] = i2c_smbus_read_byte_data(client,0x03);
	data_buf[4] = i2c_smbus_read_byte_data(client,0x04);
	for(i = 0,checksum = 0;i < 4;i++) {
                checksum += data_buf[i];
        }
	if(checksum == data_buf[4]) {
		if(temp) {
			data->temperature.val = data_buf[2];
			data->temperature.val2 = data_buf[3];
		}
		else {
			data->humidity.val = data_buf[0];
                        data->humidity.val2 = data_buf[1];
		}
	}
	else
		ret = -EINVAL;

	return ret;	
}

static int dht12_read_raw(struct iio_dev *iio_dev,
                          const struct iio_chan_spec *chan,
                        int *val, int *val2, long mask)
{
	int ret;
	struct dht12 *data = iio_priv(iio_dev);
	//use i2c protocal to read humidity or temp or humidity
    switch (mask) {
    case IIO_CHAN_INFO_PROCESSED:
			mutex_lock(&data->lock);
			ret = dht12_read_measurement(data, chan->type == IIO_TEMP);
			mutex_unlock(&data->lock);
			if (ret < 0)
                    return ret;
		if( chan->type == IIO_TEMP) {
			*val = (int) data->temperature.val;
			*val2 = (int) data->temperature.val2;
		}
		else if( chan->type == IIO_HUMIDITYRELATIVE) { // linux kernel 3.10.79 don't support IIO_HUMIDITYRELATIVE,
			*val = (int) data->humidity.val;      //so you should patch CUSTOM_IIO_HUMIDITYRELATIVE.patch
			*val2 = (int) data->humidity.val2;
		}
		else
			return -EINVAL;
		return IIO_VAL_INT_PLUS_MICRO;
    default:
            break;
     }
    return -EINVAL;

}
static const struct iio_info dht12_iio_info = {
        .driver_module          = THIS_MODULE,
        .read_raw               = dht12_read_raw,
};

static const struct iio_chan_spec dht12_chan_spec[] = {
        { 
		.type = IIO_TEMP,
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
	},
        {
		.type = IIO_HUMIDITYRELATIVE,
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED), 
	}
};

//just for debug
static ssize_t dht12_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	int i,checksum;
	unsigned char data_buf[5];
	mutex_lock(&my_mutex);
	data_buf[0] = i2c_smbus_read_byte_data(dht12_client,0x00);
        data_buf[1] = i2c_smbus_read_byte_data(dht12_client,0x01);
	data_buf[2] = i2c_smbus_read_byte_data(dht12_client,0x02);
	data_buf[3] = i2c_smbus_read_byte_data(dht12_client,0x03);
	data_buf[4] = i2c_smbus_read_byte_data(dht12_client,0x04);
	for(i = 0,checksum = 0;i < 4;i++) {
		checksum += data_buf[i];
	}
        mutex_unlock(&my_mutex);
	if(checksum == data_buf[4])
		return sprintf(buf,"Humi = %d.%d %%RH, Temp = %d.%d degree, checksum = %d \n",data_buf[0] , data_buf[1], data_buf[2], data_buf[3], data_buf[4]);
	else
		return sprintf(buf,"checksum error\n");

}
static DEVICE_ATTR(data, S_IRUGO | S_IWUSR, dht12_show, NULL);
static struct attribute *dht12_attrs[] = {
    &dev_attr_data.attr,
    NULL
};
static struct attribute_group mydrv_attr_group = {
    .name = "dht12_sysfs",
    .attrs = dht12_attrs,
};

static int dht12_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	int ret;
	struct device *dev = &i2c->dev;
        struct device_node *node = dev->of_node;
        struct dht12 *dht12;
        struct iio_dev *iio;
	//add iio subsystem
	iio = iio_device_alloc(sizeof(*dht12));
        if (!iio) {
                dev_err(dev, "Failed to allocate IIO device\n");
                return -ENOMEM;
        }
	dht12 = iio_priv(iio);
        dht12->dev = dev;
	dht12->client = i2c;
	i2c_set_clientdata(i2c, iio);
	mutex_init(&dht12->lock);
        iio->name = i2c->name;
        iio->dev.parent = &i2c->dev;
        iio->info = &dht12_iio_info;
        iio->modes = INDIO_DIRECT_MODE;
        iio->channels = dht12_chan_spec;
        iio->num_channels = ARRAY_SIZE(dht12_chan_spec);

	dev_dbg(&i2c->dev, "%s\n", __func__);
	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL))
                return -ENODEV;
	dev_info(&i2c->dev, "chip found, driver version " DRV_VERSION "\n");
	dht12_client = i2c;
	mutex_init(&my_mutex);
	printk("dht12 device component found!~\n");
	ret = sysfs_create_group(&i2c->dev.kobj, &mydrv_attr_group);
	
	return iio_device_register(iio);
}
static int dht12_remove(struct i2c_client *i2c)
{
	sysfs_remove_group(&i2c->dev.kobj, &mydrv_attr_group);
	iio_device_unregister(i2c_get_clientdata(i2c));
        iio_device_free(i2c_get_clientdata(i2c));
	return 0;
}
static const struct i2c_device_id dht12_id[] = {  
    { "dht12", 0},
    {}
};
MODULE_DEVICE_TABLE(i2c, dht12_id);

static struct of_device_id dht12_of_match[] = {
        { .compatible = "aosong,dht12" },
        {},
};
MODULE_DEVICE_TABLE(of, dht12_of_match);
struct i2c_driver dht12_driver = {
    .driver = {
        .name           = "dht12",
        .owner          = THIS_MODULE,
        .of_match_table = of_match_ptr(dht12_of_match),
    },
    .probe      = dht12_probe,
    .remove     = dht12_remove,
    .id_table   = dht12_id,
};
module_i2c_driver(dht12_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kevin.Shen");
MODULE_DESCRIPTION("A i2c-dht12 driver for testing module ");
MODULE_VERSION(DRV_VERSION);
