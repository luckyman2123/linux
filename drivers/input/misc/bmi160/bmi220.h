/**
* Copyright (c) 2020 Bosch Sensortec GmbH. All rights reserved.
*
* BSD-3-Clause
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its
*    contributors may be used to endorse or promote products derived from
*    this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
* COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
* @file	bmi220.h
* @date	2020-01-24
* @version	v2.47.0
*
*/
#ifndef BMI220_H_
#define BMI220_H_

/*! CPP guard */
#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************/

/*!             Header files
 ****************************************************************************/
#include "bmi2.h"

/***************************************************************************/

/*!               Macro definitions
 ****************************************************************************/

/*! @name BMI220 chip identifier */
#define BMI220_CHIP_ID                       UINT8_C(0x26)

/*! @name BMI220 feature input start addresses */
#define BMI220_CONFIG_ID_STRT_ADDR           UINT8_C(0x04)
#define BMI220_STEP_COUNT_STRT_ADDR          UINT8_C(0x00)
#define BMI220_MAX_BURST_LEN_STRT_ADDR       UINT8_C(0x06)
#define BMI220_CRT_GYRO_SELF_TEST_STRT_ADDR  UINT8_C(0x07)
#define BMI220_ABORT_STRT_ADDR               UINT8_C(0x07)
#define BMI220_AXIS_MAP_STRT_ADDR            UINT8_C(0x08)
#define BMI220_ACCEL_SELF_TEST_STRT_ADDR     UINT8_C(0x09)
#define BMI220_NVM_PROG_PREP_STRT_ADDR       UINT8_C(0x09)
#define BMI220_GYRO_GAIN_UPDATE_STRT_ADDR    UINT8_C(0x0A)
#define BMI220_ANY_MOT_STRT_ADDR             UINT8_C(0x00)
#define BMI220_NO_MOT_STRT_ADDR              UINT8_C(0x04)

/*! @name BMI220 feature output start addresses */
#define BMI220_STEP_CNT_OUT_STRT_ADDR        UINT8_C(0x00)
#define BMI220_GYR_USER_GAIN_OUT_STRT_ADDR   UINT8_C(0x04)
#define BMI220_ACCEL_SELF_TEST_OUT_STRT_ADDR UINT8_C(0x06)
#define BMI220_GYRO_CROSS_SENSE_STRT_ADDR    UINT8_C(0x0C)
#define BMI220_NVM_VFRM_OUT_STRT_ADDR        UINT8_C(0x0E)

/*! @name Defines maximum number of pages */
#define BMI220_NUMBER_OF_PAGES               UINT8_C(4)

/*! @name Defines maximum number of feature input configurations */
#define BMI220_MAX_FEAT_IN                   UINT8_C(12)

/*! @name Defines maximum number of feature outputs */
#define BMI220_MAX_FEAT_OUT                  UINT8_C(6)

/*! @name Mask definitions for feature interrupt status bits */
#define BMI220_STEP_CNT_STATUS_MASK          UINT8_C(0x02)
#define BMI220_NO_MOT_STATUS_MASK            UINT8_C(0x20)
#define BMI220_ANY_MOT_STATUS_MASK           UINT8_C(0x40)

/***************************************************************************/

/*!     BMI220 User Interface function prototypes
 ****************************************************************************/

/*!
 *  @brief This API:
 *  1) updates the device structure with address of the configuration file.
 *  2) Initializes BMI220 sensor.
 *  3) Writes the configuration file.
 *  4) Updates the feature offset parameters in the device structure.
 *  5) Updates the maximum number of pages, in the device structure.
 *
 * @param[in, out] dev      : Structure instance of bmi2_dev.
 *
 * @return Result of API execution status
 *
 * @retval BMI2_OK - Success
 * @retval BMI2_E_NULL_PTR - Error: Null pointer error
 * @retval BMI2_E_COM_FAIL - Error: Communication fail
 * @retval BMI2_E_DEV_NOT_FOUND - Invalid device
 */
int8_t bmi220_init(struct bmi2_dev *dev);

/******************************************************************************/
/*! @name       C++ Guard Macros                                      */
/******************************************************************************/
#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* BMI220_H_ */
