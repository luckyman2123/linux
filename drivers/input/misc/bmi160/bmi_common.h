#ifndef _BMI_COMMON_H
#define _BMI_COMMON_H

#include <linux/types.h>
#include <linux/module.h>

#define SENSOR_NAME "bmi160"
#define A_BYTES_FRM      (6)
#define G_BYTES_FRM      (6)
#define M_BYTES_FRM      (8)
#define MA_BYTES_FRM     (14)
#define MG_BYTES_FRM     (14)

struct pw_mode {
	u8 acc_pm;
	u8 gyro_pm;
	u8 mag_pm;
};

#endif /* _BMI_COMMON_H  */
