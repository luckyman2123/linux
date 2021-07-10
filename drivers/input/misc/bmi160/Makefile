#
# Makefile for Bosch sensor driver.
#
obj-y  += bmi160_i2c.o
obj-y    += bmi160_driver.o bmi160.o
EXTRA_CFLAGS += -DBMI_USE_BASIC_I2C_FUNC
#EXTRA_CFLAGS += -DCONFIG_USE_QUALCOMM_HAL
EXTRA_CFLAGS += -DBMI160_ENABLE_INT1
EXTRA_CFLAGS += -DBMI160_ENABLE_INT2

obj-y += bs_log.o
obj-y    += bmi2xy_driver.o bmi2.o bmi220.o
#obj-y    += bmi2xy_i2c.o
EXTRA_CFLAGS += -DBMI2XY_LOAD_CONFIG_FILE_IN_INIT
