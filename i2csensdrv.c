/******************************************************************************
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 2 of the License.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *****************************************************************************/

#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/of.h>

#define DEVICE_FILE_NAME	"i2csens"
#define DRIVER_NAME				"i2csensdrv"

#define I2CSENS_REG_ID			0
#define I2CSENS_REG_CTRL		1
#define I2CSENS_REG_DATA		2

#define CTRL_EN_MASK				0x1

#define I2CSENS_ID				0x5Au

/**
 * struct i2csens - i2csens device private data structure
 * regmap - register map of the I2C peripheral
 */
struct i2csens {
	struct regmap *regmap;
};

/* SYSFS attributes */

/**
 * Enable device getter.
 */
static ssize_t enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2csens *data = dev_get_drvdata(dev);
	unsigned int enable;
	int err;

	err = regmap_read(data->regmap, I2CSENS_REG_CTRL, &enable);
	enable &= CTRL_EN_MASK;

	return sprintf(buf, "%d\n", !!enable);
}

/**
 * Enable device setter.
 */
static ssize_t enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2csens *data = dev_get_drvdata(dev);
	unsigned int ctrl;
	int enable;
	int err;

	err = regmap_read(data->regmap, I2CSENS_REG_CTRL, &ctrl);

	sscanf(buf, "%d", &enable);

	if (!enable) {
		ctrl &= ~CTRL_EN_MASK;
	} else {
		ctrl |= CTRL_EN_MASK;
	}

	err = regmap_write(data->regmap, I2CSENS_REG_CTRL, ctrl);
	if (err < 0)
		return err;

	return count;
}
static DEVICE_ATTR_RW(enable);

/**
 * Return temperature in milicelsius
 */
static ssize_t data_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2csens *data = dev_get_drvdata(dev);
	unsigned int temperature;
	int err;

	err = regmap_read(data->regmap, I2CSENS_REG_DATA, &temperature);
	temperature *= 1000;
	temperature >>= 1;

	return sprintf(buf, "%d\n", temperature);
}
static DEVICE_ATTR_RO(data);

static struct attribute *i2csens_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_data.attr,
	NULL,
};

static const struct attribute_group i2csens_group = {
	.attrs = i2csens_attrs,
};

/**
 * Device tree match table.
 */
static const struct of_device_id i2csens_of_match[] = {
	{ .compatible = "mistra,i2csens", },
	{ /* end of table */ }
};
MODULE_DEVICE_TABLE(of, i2csens_of_match);

static bool i2csens_regmap_is_writeable(struct device *dev, unsigned int reg)
{
	return reg == I2CSENS_REG_CTRL;
}

static const struct regmap_config i2csens_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
	.max_register = I2CSENS_REG_DATA,
	.writeable_reg = i2csens_regmap_is_writeable,
};

/**
 * Driver probe function.
 * Configure I2C client structure and set up driver.
 */
static int i2csens_probe(struct i2c_client *client)
{
	struct i2csens *data;
	unsigned int regval;
	int status;

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	i2c_set_clientdata(client, data);

	data->regmap = devm_regmap_init_i2c(client, &i2csens_regmap_config);
	if (IS_ERR(data->regmap)) {
		dev_err(&client->dev, "failed to allocate register map\n");
		return PTR_ERR(data->regmap);
	}

	status = regmap_read(data->regmap, I2CSENS_REG_ID, &regval);
	if (status < 0) {
		dev_err(&client->dev, "error reading ID register\n");
		return status;
	}

	if (regval != I2CSENS_ID) {
		dev_err(&client->dev, "unexpected ID\n");
		return -ENODEV;
	}

	status = sysfs_create_group(&client->dev.kobj, &i2csens_group);
	if (status) {
		dev_info(&client->dev, "Cannot create sysfs\n");
	}

	return 0;
}

static const struct i2c_device_id i2csens_id[] = {
	{ "i2csens", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, i2csens_id);

static struct i2c_driver i2csens_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = i2csens_of_match,
	},
	.probe = i2csens_probe,
	.id_table = i2csens_id,
};
module_i2c_driver(i2csens_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Custom QEMU I2C sensor Driver and Device");
MODULE_AUTHOR("Strahinja Jankovic");
