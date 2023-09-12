/*
 * This driver supports the controls for X-Powers (Allwinner)
 * AC100 audio codec. This codec is co-packaged with AXP81x PMICs.
 *
 * (C) Copyright 2020 Ondrej Jirman <megi@xff.cz>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/mfd/ac100.h>

#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/soc-dapm.h>

#define AC100_ADC_APC_CTRL_ADCR_EN_OFF                          15
#define AC100_ADC_APC_CTRL_ADCR_EN_MASK                         BIT(15)
#define AC100_ADC_APC_CTRL_ADCR_EN_DISABLED                     0
#define AC100_ADC_APC_CTRL_ADCR_EN_ENABLED                      BIT(15)
#define AC100_ADC_APC_CTRL_ADCR_GAIN_OFF                        12
#define AC100_ADC_APC_CTRL_ADCR_GAIN(v)                         (((v) & 0x7) << 12)
#define AC100_ADC_APC_CTRL_ADCL_EN_OFF                          11
#define AC100_ADC_APC_CTRL_ADCL_EN_MASK                         BIT(11)
#define AC100_ADC_APC_CTRL_ADCL_EN_DISABLED                     0
#define AC100_ADC_APC_CTRL_ADCL_EN_ENABLED                      BIT(11)
#define AC100_ADC_APC_CTRL_ADCL_GAIN_OFF                        8
#define AC100_ADC_APC_CTRL_ADCL_GAIN(v)                         (((v) & 0x7) << 8)
#define AC100_ADC_APC_CTRL_MBIAS_EN_OFF                         7
#define AC100_ADC_APC_CTRL_MBIAS_EN_MASK                        BIT(7)
#define AC100_ADC_APC_CTRL_MBIAS_EN_DISABLED                    0
#define AC100_ADC_APC_CTRL_MBIAS_EN_ENABLED                     BIT(7)
#define AC100_ADC_APC_CTRL_MMIC_BIAS_CHOPPER_EN_OFF             6
#define AC100_ADC_APC_CTRL_MMIC_BIAS_CHOPPER_EN_MASK            BIT(6)
#define AC100_ADC_APC_CTRL_MMIC_BIAS_CHOPPER_EN_DISABLED        0
#define AC100_ADC_APC_CTRL_MMIC_BIAS_CHOPPER_EN_ENABLED         BIT(6)
#define AC100_ADC_APC_CTRL_MMIC_BIAS_CHOPPER_CKS_OFF            4
#define AC100_ADC_APC_CTRL_MMIC_BIAS_CHOPPER_CKS_MASK           GENMASK(5, 4)
#define AC100_ADC_APC_CTRL_MMIC_BIAS_CHOPPER_CKS_250K           (0x0 << 4)
#define AC100_ADC_APC_CTRL_MMIC_BIAS_CHOPPER_CKS_500K           (0x1 << 4)
#define AC100_ADC_APC_CTRL_MMIC_BIAS_CHOPPER_CKS_1M             (0x2 << 4)
#define AC100_ADC_APC_CTRL_MMIC_BIAS_CHOPPER_CKS_2M             (0x3 << 4)
#define AC100_ADC_APC_CTRL_HBIAS_MODE_OFF                       2
#define AC100_ADC_APC_CTRL_HBIAS_MODE_MASK                      BIT(2)
#define AC100_ADC_APC_CTRL_HBIAS_MODE_LOAD                      0
#define AC100_ADC_APC_CTRL_HBIAS_MODE_HBIAS_EN                  BIT(2)
#define AC100_ADC_APC_CTRL_HBIAS_EN_OFF                         1
#define AC100_ADC_APC_CTRL_HBIAS_EN_MASK                        BIT(1)
#define AC100_ADC_APC_CTRL_HBIAS_EN_DISABLED                    0
#define AC100_ADC_APC_CTRL_HBIAS_EN_ENABLED                     BIT(1)
#define AC100_ADC_APC_CTRL_HBIAS_ADC_EN_OFF                     0
#define AC100_ADC_APC_CTRL_HBIAS_ADC_EN_MASK                    BIT(0)
#define AC100_ADC_APC_CTRL_HBIAS_ADC_EN_DISABLED                0
#define AC100_ADC_APC_CTRL_HBIAS_ADC_EN_ENABLED                 BIT(0)

#define AC100_ADC_SRC_ADCR_MIC1_BOOST_OFF                       13
#define AC100_ADC_SRC_ADCR_MIC1_BOOST_MASK                      BIT(13)
#define AC100_ADC_SRC_ADCR_MIC1_BOOST_DISABLED                  0
#define AC100_ADC_SRC_ADCR_MIC1_BOOST_ENABLED                   BIT(13)
#define AC100_ADC_SRC_ADCR_MIC2_BOOST_OFF                       12
#define AC100_ADC_SRC_ADCR_MIC2_BOOST_MASK                      BIT(12)
#define AC100_ADC_SRC_ADCR_MIC2_BOOST_DISABLED                  0
#define AC100_ADC_SRC_ADCR_MIC2_BOOST_ENABLED                   BIT(12)
#define AC100_ADC_SRC_ADCR_LINEINL_LINEINR_OFF                  11
#define AC100_ADC_SRC_ADCR_LINEINL_LINEINR_MASK                 BIT(11)
#define AC100_ADC_SRC_ADCR_LINEINL_LINEINR_DISABLED             0
#define AC100_ADC_SRC_ADCR_LINEINL_LINEINR_ENABLED              BIT(11)
#define AC100_ADC_SRC_ADCR_LINEINR_OFF                          10
#define AC100_ADC_SRC_ADCR_LINEINR_MASK                         BIT(10)
#define AC100_ADC_SRC_ADCR_LINEINR_DISABLED                     0
#define AC100_ADC_SRC_ADCR_LINEINR_ENABLED                      BIT(10)
#define AC100_ADC_SRC_ADCR_AUXINR_OFF                           9
#define AC100_ADC_SRC_ADCR_AUXINR_MASK                          BIT(9)
#define AC100_ADC_SRC_ADCR_AUXINR_DISABLED                      0
#define AC100_ADC_SRC_ADCR_AUXINR_ENABLED                       BIT(9)
#define AC100_ADC_SRC_ADCR_ROUTMIX_OFF                          8
#define AC100_ADC_SRC_ADCR_ROUTMIX_MASK                         BIT(8)
#define AC100_ADC_SRC_ADCR_ROUTMIX_DISABLED                     0
#define AC100_ADC_SRC_ADCR_ROUTMIX_ENABLED                      BIT(8)
#define AC100_ADC_SRC_ADCR_LOUTMIX_OFF                          7
#define AC100_ADC_SRC_ADCR_LOUTMIX_MASK                         BIT(7)
#define AC100_ADC_SRC_ADCR_LOUTMIX_DISABLED                     0
#define AC100_ADC_SRC_ADCR_LOUTMIX_ENABLED                      BIT(7)
#define AC100_ADC_SRC_ADCL_MIC1_BOOST_OFF                       6
#define AC100_ADC_SRC_ADCL_MIC1_BOOST_MASK                      BIT(6)
#define AC100_ADC_SRC_ADCL_MIC1_BOOST_DISABLED                  0
#define AC100_ADC_SRC_ADCL_MIC1_BOOST_ENABLED                   BIT(6)
#define AC100_ADC_SRC_ADCL_MIC2_BOOST_OFF                       5
#define AC100_ADC_SRC_ADCL_MIC2_BOOST_MASK                      BIT(5)
#define AC100_ADC_SRC_ADCL_MIC2_BOOST_DISABLED                  0
#define AC100_ADC_SRC_ADCL_MIC2_BOOST_ENABLED                   BIT(5)
#define AC100_ADC_SRC_ADCL_LINEINL_LINEINR_OFF                  4
#define AC100_ADC_SRC_ADCL_LINEINL_LINEINR_MASK                 BIT(4)
#define AC100_ADC_SRC_ADCL_LINEINL_LINEINR_DISABLED             0
#define AC100_ADC_SRC_ADCL_LINEINL_LINEINR_ENABLED              BIT(4)
#define AC100_ADC_SRC_ADCL_LINEINL_OFF                          3
#define AC100_ADC_SRC_ADCL_LINEINL_MASK                         BIT(3)
#define AC100_ADC_SRC_ADCL_LINEINL_DISABLED                     0
#define AC100_ADC_SRC_ADCL_LINEINL_ENABLED                      BIT(3)
#define AC100_ADC_SRC_ADCL_AUXINL_OFF                           2
#define AC100_ADC_SRC_ADCL_AUXINL_MASK                          BIT(2)
#define AC100_ADC_SRC_ADCL_AUXINL_DISABLED                      0
#define AC100_ADC_SRC_ADCL_AUXINL_ENABLED                       BIT(2)
#define AC100_ADC_SRC_ADCL_LOUTMIX_OFF                          1
#define AC100_ADC_SRC_ADCL_LOUTMIX_MASK                         BIT(1)
#define AC100_ADC_SRC_ADCL_LOUTMIX_DISABLED                     0
#define AC100_ADC_SRC_ADCL_LOUTMIX_ENABLED                      BIT(1)
#define AC100_ADC_SRC_ADCL_ROUTMIX_OFF                          0
#define AC100_ADC_SRC_ADCL_ROUTMIX_MASK                         BIT(0)
#define AC100_ADC_SRC_ADCL_ROUTMIX_DISABLED                     0
#define AC100_ADC_SRC_ADCL_ROUTMIX_ENABLED                      BIT(0)

#define AC100_ADC_SRC_BST_CTRL_MIC1AMPEN_OFF                    15
#define AC100_ADC_SRC_BST_CTRL_MIC1AMPEN_MASK                   BIT(15)
#define AC100_ADC_SRC_BST_CTRL_MIC1AMPEN_DISABLED               0
#define AC100_ADC_SRC_BST_CTRL_MIC1AMPEN_ENABLED                BIT(15)
#define AC100_ADC_SRC_BST_CTRL_MIC1BOOST_OFF                    12
#define AC100_ADC_SRC_BST_CTRL_MIC1BOOST(v)                     (((v) & 0x7) << 12)
#define AC100_ADC_SRC_BST_CTRL_MIC2AMPEN_OFF                    11
#define AC100_ADC_SRC_BST_CTRL_MIC2AMPEN_MASK                   BIT(11)
#define AC100_ADC_SRC_BST_CTRL_MIC2AMPEN_DISABLED               0
#define AC100_ADC_SRC_BST_CTRL_MIC2AMPEN_ENABLED                BIT(11)
#define AC100_ADC_SRC_BST_CTRL_MIC2BOOST_OFF                    8
#define AC100_ADC_SRC_BST_CTRL_MIC2BOOST(v)                     (((v) & 0x7) << 8)
#define AC100_ADC_SRC_BST_CTRL_MIC2SLT_OFF                      7
#define AC100_ADC_SRC_BST_CTRL_MIC2SLT_MASK                     BIT(7)
#define AC100_ADC_SRC_BST_CTRL_MIC2SLT_MIC2                     0
#define AC100_ADC_SRC_BST_CTRL_MIC2SLT_MIC3                     BIT(7)
#define AC100_ADC_SRC_BST_CTRL_LINEIN_DIFF_PREG_OFF             4
#define AC100_ADC_SRC_BST_CTRL_LINEIN_DIFF_PREG(v)              (((v) & 0x7) << 4)
#define AC100_ADC_SRC_BST_CTRL_AXI_PREG_OFF                     0
#define AC100_ADC_SRC_BST_CTRL_AXI_PREG(v)                      ((v) & 0x7)

#define AC100_OUT_MXR_DAC_A_CTRL_DAC_AR_EN_OFF                  15
#define AC100_OUT_MXR_DAC_A_CTRL_DAC_AR_EN_MASK                 BIT(15)
#define AC100_OUT_MXR_DAC_A_CTRL_DAC_AR_EN_DISABLED             0
#define AC100_OUT_MXR_DAC_A_CTRL_DAC_AR_EN_ENABLED              BIT(15)
#define AC100_OUT_MXR_DAC_A_CTRL_DAC_AL_EN_OFF                  14
#define AC100_OUT_MXR_DAC_A_CTRL_DAC_AL_EN_MASK                 BIT(14)
#define AC100_OUT_MXR_DAC_A_CTRL_DAC_AL_EN_DISABLED             0
#define AC100_OUT_MXR_DAC_A_CTRL_DAC_AL_EN_ENABLED              BIT(14)
#define AC100_OUT_MXR_DAC_A_CTRL_AR_MIX_EN_OFF                  13
#define AC100_OUT_MXR_DAC_A_CTRL_AR_MIX_EN_MASK                 BIT(13)
#define AC100_OUT_MXR_DAC_A_CTRL_AR_MIX_EN_DISABLED             0
#define AC100_OUT_MXR_DAC_A_CTRL_AR_MIX_EN_ENABLED              BIT(13)
#define AC100_OUT_MXR_DAC_A_CTRL_AL_MIX_EN_OFF                  12
#define AC100_OUT_MXR_DAC_A_CTRL_AL_MIX_EN_MASK                 BIT(12)
#define AC100_OUT_MXR_DAC_A_CTRL_AL_MIX_EN_DISABLED             0
#define AC100_OUT_MXR_DAC_A_CTRL_AL_MIX_EN_ENABLED              BIT(12)
#define AC100_OUT_MXR_DAC_A_CTRL_HP_DCRM_EN_OFF                 8
#define AC100_OUT_MXR_DAC_A_CTRL_HP_DCRM_EN(v)                  (((v) & 0xf) << 8)

#define AC100_OUT_MXR_SRC_RMIX_MIC1_BOOST_OFF                   13
#define AC100_OUT_MXR_SRC_RMIX_MIC1_BOOST_MASK                  BIT(13)
#define AC100_OUT_MXR_SRC_RMIX_MIC1_BOOST_DISABLED              0
#define AC100_OUT_MXR_SRC_RMIX_MIC1_BOOST_ENABLED               BIT(13)
#define AC100_OUT_MXR_SRC_RMIX_MIC2_BOOST_OFF                   12
#define AC100_OUT_MXR_SRC_RMIX_MIC2_BOOST_MASK                  BIT(12)
#define AC100_OUT_MXR_SRC_RMIX_MIC2_BOOST_DISABLED              0
#define AC100_OUT_MXR_SRC_RMIX_MIC2_BOOST_ENABLED               BIT(12)
#define AC100_OUT_MXR_SRC_RMIX_LINEINL_LINEINR_OFF              11
#define AC100_OUT_MXR_SRC_RMIX_LINEINL_LINEINR_MASK             BIT(11)
#define AC100_OUT_MXR_SRC_RMIX_LINEINL_LINEINR_DISABLED         0
#define AC100_OUT_MXR_SRC_RMIX_LINEINL_LINEINR_ENABLED          BIT(11)
#define AC100_OUT_MXR_SRC_RMIX_LINEINR_OFF                      10
#define AC100_OUT_MXR_SRC_RMIX_LINEINR_MASK                     BIT(10)
#define AC100_OUT_MXR_SRC_RMIX_LINEINR_DISABLED                 0
#define AC100_OUT_MXR_SRC_RMIX_LINEINR_ENABLED                  BIT(10)
#define AC100_OUT_MXR_SRC_RMIX_AUXINR_OFF                       9
#define AC100_OUT_MXR_SRC_RMIX_AUXINR_MASK                      BIT(9)
#define AC100_OUT_MXR_SRC_RMIX_AUXINR_DISABLED                  0
#define AC100_OUT_MXR_SRC_RMIX_AUXINR_ENABLED                   BIT(9)
#define AC100_OUT_MXR_SRC_RMIX_DACR_OFF                         8
#define AC100_OUT_MXR_SRC_RMIX_DACR_MASK                        BIT(8)
#define AC100_OUT_MXR_SRC_RMIX_DACR_DISABLED                    0
#define AC100_OUT_MXR_SRC_RMIX_DACR_ENABLED                     BIT(8)
#define AC100_OUT_MXR_SRC_RMIX_DACL_OFF                         7
#define AC100_OUT_MXR_SRC_RMIX_DACL_MASK                        BIT(7)
#define AC100_OUT_MXR_SRC_RMIX_DACL_DISABLED                    0
#define AC100_OUT_MXR_SRC_RMIX_DACL_ENABLED                     BIT(7)
#define AC100_OUT_MXR_SRC_LMIX_MIC1_BOOST_OFF                   6
#define AC100_OUT_MXR_SRC_LMIX_MIC1_BOOST_MASK                  BIT(6)
#define AC100_OUT_MXR_SRC_LMIX_MIC1_BOOST_DISABLED              0
#define AC100_OUT_MXR_SRC_LMIX_MIC1_BOOST_ENABLED               BIT(6)
#define AC100_OUT_MXR_SRC_LMIX_MIC2_BOOST_OFF                   5
#define AC100_OUT_MXR_SRC_LMIX_MIC2_BOOST_MASK                  BIT(5)
#define AC100_OUT_MXR_SRC_LMIX_MIC2_BOOST_DISABLED              0
#define AC100_OUT_MXR_SRC_LMIX_MIC2_BOOST_ENABLED               BIT(5)
#define AC100_OUT_MXR_SRC_LMIX_LINEINL_LINEINR_OFF              4
#define AC100_OUT_MXR_SRC_LMIX_LINEINL_LINEINR_MASK             BIT(4)
#define AC100_OUT_MXR_SRC_LMIX_LINEINL_LINEINR_DISABLED         0
#define AC100_OUT_MXR_SRC_LMIX_LINEINL_LINEINR_ENABLED          BIT(4)
#define AC100_OUT_MXR_SRC_LMIX_LINEINL_OFF                      3
#define AC100_OUT_MXR_SRC_LMIX_LINEINL_MASK                     BIT(3)
#define AC100_OUT_MXR_SRC_LMIX_LINEINL_DISABLED                 0
#define AC100_OUT_MXR_SRC_LMIX_LINEINL_ENABLED                  BIT(3)
#define AC100_OUT_MXR_SRC_LMIX_AUXINL_OFF                       2
#define AC100_OUT_MXR_SRC_LMIX_AUXINL_MASK                      BIT(2)
#define AC100_OUT_MXR_SRC_LMIX_AUXINL_DISABLED                  0
#define AC100_OUT_MXR_SRC_LMIX_AUXINL_ENABLED                   BIT(2)
#define AC100_OUT_MXR_SRC_LMIX_DACL_OFF                         1
#define AC100_OUT_MXR_SRC_LMIX_DACL_MASK                        BIT(1)
#define AC100_OUT_MXR_SRC_LMIX_DACL_DISABLED                    0
#define AC100_OUT_MXR_SRC_LMIX_DACL_ENABLED                     BIT(1)
#define AC100_OUT_MXR_SRC_LMIX_DACR_OFF                         0
#define AC100_OUT_MXR_SRC_LMIX_DACR_MASK                        BIT(0)
#define AC100_OUT_MXR_SRC_LMIX_DACR_DISABLED                    0
#define AC100_OUT_MXR_SRC_LMIX_DACR_ENABLED                     BIT(0)

#define AC100_OUT_MXR_SRC_BST_HMICBIAS_VOLTAGE_OFF              14
#define AC100_OUT_MXR_SRC_BST_HMICBIAS_VOLTAGE_MASK             GENMASK(15, 14)
#define AC100_OUT_MXR_SRC_BST_HMICBIAS_VOLTAGE_1_88V            (0x0 << 14)
#define AC100_OUT_MXR_SRC_BST_HMICBIAS_VOLTAGE_2_09V            (0x1 << 14)
#define AC100_OUT_MXR_SRC_BST_HMICBIAS_VOLTAGE_2_33V            (0x2 << 14)
#define AC100_OUT_MXR_SRC_BST_HMICBIAS_VOLTAGE_2_50V            (0x3 << 14)
#define AC100_OUT_MXR_SRC_BST_MMICBIAS_VOLTAGE_OFF              12
#define AC100_OUT_MXR_SRC_BST_MMICBIAS_VOLTAGE_MASK             GENMASK(13, 12)
#define AC100_OUT_MXR_SRC_BST_MMICBIAS_VOLTAGE_1_88V            (0x0 << 12)
#define AC100_OUT_MXR_SRC_BST_MMICBIAS_VOLTAGE_2_09V            (0x1 << 12)
#define AC100_OUT_MXR_SRC_BST_MMICBIAS_VOLTAGE_2_33V            (0x2 << 12)
#define AC100_OUT_MXR_SRC_BST_MMICBIAS_VOLTAGE_2_50V            (0x3 << 12)
#define AC100_OUT_MXR_SRC_BST_AX_GAIN_OFF                       9
#define AC100_OUT_MXR_SRC_BST_AX_GAIN(v)                        (((v) & 0x7) << 9)
#define AC100_OUT_MXR_SRC_BST_MIC1_GAIN_OFF                     6
#define AC100_OUT_MXR_SRC_BST_MIC1_GAIN(v)                      (((v) & 0x7) << 6)
#define AC100_OUT_MXR_SRC_BST_MIC2_GAIN_OFF                     3
#define AC100_OUT_MXR_SRC_BST_MIC2_GAIN(v)                      (((v) & 0x7) << 3)
#define AC100_OUT_MXR_SRC_BST_LINEIN_GAIN_OFF                   0
#define AC100_OUT_MXR_SRC_BST_LINEIN_GAIN(v)                    ((v) & 0x7)

#define AC100_HPOUT_CTRL_RIGHT_SRC_OFF                          15
#define AC100_HPOUT_CTRL_RIGHT_SRC_MASK                         BIT(15)
#define AC100_HPOUT_CTRL_RIGHT_SRC_DACR                         0
#define AC100_HPOUT_CTRL_RIGHT_SRC_RAMIX                        BIT(15)
#define AC100_HPOUT_CTRL_LEFT_SRC_OFF                           14
#define AC100_HPOUT_CTRL_LEFT_SRC_MASK                          BIT(14)
#define AC100_HPOUT_CTRL_LEFT_SRC_DACL                          0
#define AC100_HPOUT_CTRL_LEFT_SRC_LAMIX                         BIT(14)
#define AC100_HPOUT_CTRL_RIGHT_PA_MUTE_OFF                      13
#define AC100_HPOUT_CTRL_RIGHT_PA_MUTE_MASK                     BIT(13)
#define AC100_HPOUT_CTRL_RIGHT_PA_MUTE_MUTE                     0
#define AC100_HPOUT_CTRL_RIGHT_PA_MUTE_NOT_MUTE                 BIT(13)
#define AC100_HPOUT_CTRL_LEFT_PA_MUTE_OFF                       12
#define AC100_HPOUT_CTRL_LEFT_PA_MUTE_MASK                      BIT(12)
#define AC100_HPOUT_CTRL_LEFT_PA_MUTE_MUTE                      0
#define AC100_HPOUT_CTRL_LEFT_PA_MUTE_NOT_MUTE                  BIT(12)
#define AC100_HPOUT_CTRL_PA_EN_OFF                              11
#define AC100_HPOUT_CTRL_PA_EN_MASK                             BIT(11)
#define AC100_HPOUT_CTRL_PA_EN_DISABLED                         0
#define AC100_HPOUT_CTRL_PA_EN_ENABLED                          BIT(11)
#define AC100_HPOUT_CTRL_VOLUME_OFF                             4
#define AC100_HPOUT_CTRL_VOLUME(v)                              (((v) & 0x3f) << 4)
#define AC100_HPOUT_CTRL_STARTUP_DELAY_OFF                      2
#define AC100_HPOUT_CTRL_STARTUP_DELAY_MASK                     GENMASK(3, 2)
#define AC100_HPOUT_CTRL_STARTUP_DELAY_4ms                      (0x0 << 2)
#define AC100_HPOUT_CTRL_STARTUP_DELAY_8ms                      (0x1 << 2)
#define AC100_HPOUT_CTRL_STARTUP_DELAY_16ms                     (0x2 << 2)
#define AC100_HPOUT_CTRL_STARTUP_DELAY_32ms                     (0x3 << 2)
#define AC100_HPOUT_CTRL_OUTPUT_CURRENT_OFF                     0
#define AC100_HPOUT_CTRL_OUTPUT_CURRENT(v)                      ((v) & 0x3)

#define AC100_ERPOUT_CTRL_RAMP_TIME_OFF                         11
#define AC100_ERPOUT_CTRL_RAMP_TIME_MASK                        GENMASK(12, 11)
#define AC100_ERPOUT_CTRL_RAMP_TIME_256ms                       (0x0 << 11)
#define AC100_ERPOUT_CTRL_RAMP_TIME_512ms                       (0x1 << 11)
#define AC100_ERPOUT_CTRL_RAMP_TIME_640ms                       (0x2 << 11)
#define AC100_ERPOUT_CTRL_RAMP_TIME_768ms                       (0x3 << 11)
#define AC100_ERPOUT_CTRL_OUT_CURRENT_OFF                       9
#define AC100_ERPOUT_CTRL_OUT_CURRENT(v)                        (((v) & 0x3) << 9)
#define AC100_ERPOUT_CTRL_INPUT_SOURCE_OFF                      7
#define AC100_ERPOUT_CTRL_INPUT_SOURCE_MASK                     GENMASK(8, 7)
#define AC100_ERPOUT_CTRL_INPUT_SOURCE_DACR                     (0x0 << 7)
#define AC100_ERPOUT_CTRL_INPUT_SOURCE_DACL                     (0x1 << 7)
#define AC100_ERPOUT_CTRL_INPUT_SOURCE_RAMIX                    (0x2 << 7)
#define AC100_ERPOUT_CTRL_INPUT_SOURCE_LAMIX                    (0x3 << 7)
#define AC100_ERPOUT_CTRL_MUTE_OFF                              6
#define AC100_ERPOUT_CTRL_MUTE_MASK                             BIT(6)
#define AC100_ERPOUT_CTRL_MUTE_MUTE                             0
#define AC100_ERPOUT_CTRL_MUTE_NOT_MUTE                         BIT(6)
#define AC100_ERPOUT_CTRL_PA_EN_OFF                             5
#define AC100_ERPOUT_CTRL_PA_EN_MASK                            BIT(5)
#define AC100_ERPOUT_CTRL_PA_EN_DISABLED                        0
#define AC100_ERPOUT_CTRL_PA_EN_ENABLED                         BIT(5)
#define AC100_ERPOUT_CTRL_VOLUME_OFF                            0
#define AC100_ERPOUT_CTRL_VOLUME(v)                             ((v) & 0x1f)

#define AC100_SPKOUT_CTRL_RIGHT_SRC_OFF                         12
#define AC100_SPKOUT_CTRL_RIGHT_SRC_MASK                        BIT(12)
#define AC100_SPKOUT_CTRL_RIGHT_SRC_MIXR                        0
#define AC100_SPKOUT_CTRL_RIGHT_SRC_MIXL_MIXR                   BIT(12)
#define AC100_SPKOUT_CTRL_RIGHT_INV_EN_OFF                      11
#define AC100_SPKOUT_CTRL_RIGHT_INV_EN_MASK                     BIT(11)
#define AC100_SPKOUT_CTRL_RIGHT_INV_EN_DISABLED                 0
#define AC100_SPKOUT_CTRL_RIGHT_INV_EN_ENABLED                  BIT(11)
#define AC100_SPKOUT_CTRL_RIGHT_EN_OFF                          9
#define AC100_SPKOUT_CTRL_RIGHT_EN_MASK                         BIT(9)
#define AC100_SPKOUT_CTRL_RIGHT_EN_DISABLED                     0
#define AC100_SPKOUT_CTRL_RIGHT_EN_ENABLED                      BIT(9)
#define AC100_SPKOUT_CTRL_LEFT_SRC_OFF                          8
#define AC100_SPKOUT_CTRL_LEFT_SRC_MASK                         BIT(8)
#define AC100_SPKOUT_CTRL_LEFT_SRC_MIXL                         0
#define AC100_SPKOUT_CTRL_LEFT_SRC_MIXL_MIXR                    BIT(8)
#define AC100_SPKOUT_CTRL_LEFT_INV_EN_OFF                       7
#define AC100_SPKOUT_CTRL_LEFT_INV_EN_MASK                      BIT(7)
#define AC100_SPKOUT_CTRL_LEFT_INV_EN_DISABLED                  0
#define AC100_SPKOUT_CTRL_LEFT_INV_EN_ENABLED                   BIT(7)
#define AC100_SPKOUT_CTRL_LEFT_EN_OFF                           5
#define AC100_SPKOUT_CTRL_LEFT_EN_MASK                          BIT(5)
#define AC100_SPKOUT_CTRL_LEFT_EN_DISABLED                      0
#define AC100_SPKOUT_CTRL_LEFT_EN_ENABLED                       BIT(5)
#define AC100_SPKOUT_CTRL_VOLUME_OFF                            0
#define AC100_SPKOUT_CTRL_VOLUME(v)                             ((v) & 0x1f)

#define AC100_LINEOUT_CTRL_LINEOUT_GAIN_OFF                     5
#define AC100_LINEOUT_CTRL_LINEOUT_GAIN(v)                      (((v) & 0x7) << 5)
#define AC100_LINEOUT_CTRL_LINEOUT_EN_OFF                       4
#define AC100_LINEOUT_CTRL_LINEOUT_EN_MASK                      BIT(4)
#define AC100_LINEOUT_CTRL_LINEOUT_EN_DISABLED                  0
#define AC100_LINEOUT_CTRL_LINEOUT_EN_ENABLED                   BIT(4)
#define AC100_LINEOUT_CTRL_LINEOUT_S0_OFF                       3
#define AC100_LINEOUT_CTRL_LINEOUT_S0_MASK                      BIT(3)
#define AC100_LINEOUT_CTRL_LINEOUT_S0_MUTE                      0
#define AC100_LINEOUT_CTRL_LINEOUT_S0_ON                        BIT(3)
#define AC100_LINEOUT_CTRL_LINEOUT_S1_OFF                       2
#define AC100_LINEOUT_CTRL_LINEOUT_S1_MASK                      BIT(2)
#define AC100_LINEOUT_CTRL_LINEOUT_S1_MUTE                      0
#define AC100_LINEOUT_CTRL_LINEOUT_S1_ON                        BIT(2)
#define AC100_LINEOUT_CTRL_LINEOUT_S2_OFF                       1
#define AC100_LINEOUT_CTRL_LINEOUT_S2_MASK                      BIT(1)
#define AC100_LINEOUT_CTRL_LINEOUT_S2_MUTE                      0
#define AC100_LINEOUT_CTRL_LINEOUT_S2_ON                        BIT(1)
#define AC100_LINEOUT_CTRL_LINEOUT_S3_OFF                       0
#define AC100_LINEOUT_CTRL_LINEOUT_S3_MASK                      BIT(0)
#define AC100_LINEOUT_CTRL_LINEOUT_S3_MUTE                      0
#define AC100_LINEOUT_CTRL_LINEOUT_S3_ON                        BIT(0)

#define AC100_ADDA_TUNE1_ZERO_CROSSOVER_EN_OFF                  8
#define AC100_ADDA_TUNE1_ZERO_CROSSOVER_EN_MASK                 BIT(8)
#define AC100_ADDA_TUNE1_ZERO_CROSSOVER_EN_DIS                  0
#define AC100_ADDA_TUNE1_ZERO_CROSSOVER_EN_EN                   BIT(8)
#define AC100_ADDA_TUNE1_ZERO_CROSSOVER_TIME_OFF                7
#define AC100_ADDA_TUNE1_ZERO_CROSSOVER_TIME                    BIT(7)

struct ac100_codec {
	struct device *dev;
	struct snd_soc_component component;
};

/* ADC mixer controls */
static const struct snd_kcontrol_new ac100_codec_adc_mixer_controls[] = {
	SOC_DAPM_DOUBLE("Mic1 Capture Switch",
			AC100_ADC_SRC,
			AC100_ADC_SRC_ADCL_MIC1_BOOST_OFF,
			AC100_ADC_SRC_ADCR_MIC1_BOOST_OFF, 1, 0),
	SOC_DAPM_DOUBLE("Mic2 Capture Switch",
			AC100_ADC_SRC,
			AC100_ADC_SRC_ADCL_MIC2_BOOST_OFF,
			AC100_ADC_SRC_ADCR_MIC2_BOOST_OFF, 1, 0),
	SOC_DAPM_DOUBLE("Line In Differential Capture Switch",
			AC100_ADC_SRC,
			AC100_ADC_SRC_ADCL_LINEINL_LINEINR_OFF,
			AC100_ADC_SRC_ADCR_LINEINL_LINEINR_OFF, 1, 0),
	SOC_DAPM_DOUBLE("Line In Capture Switch",
			AC100_ADC_SRC,
			AC100_ADC_SRC_ADCL_LINEINL_OFF,
			AC100_ADC_SRC_ADCR_LINEINR_OFF, 1, 0),
	SOC_DAPM_DOUBLE("Aux In Capture Switch",
			AC100_ADC_SRC,
			AC100_ADC_SRC_ADCL_AUXINL_OFF,
			AC100_ADC_SRC_ADCR_AUXINR_OFF, 1, 0),
	SOC_DAPM_DOUBLE("Mixer Capture Switch",
			AC100_ADC_SRC,
			AC100_ADC_SRC_ADCL_LOUTMIX_OFF,
			AC100_ADC_SRC_ADCR_ROUTMIX_OFF, 1, 0),
	SOC_DAPM_DOUBLE("Mixer Reversed Capture Switch",
			AC100_ADC_SRC,
			AC100_ADC_SRC_ADCL_ROUTMIX_OFF,
			AC100_ADC_SRC_ADCR_LOUTMIX_OFF, 1, 0),
};

/* Output mixer controls */
static const struct snd_kcontrol_new ac100_codec_mixer_controls[] = {
	SOC_DAPM_DOUBLE("Mic1 Playback Switch",
			AC100_OUT_MXR_SRC,
			AC100_OUT_MXR_SRC_LMIX_MIC1_BOOST_OFF,
			AC100_OUT_MXR_SRC_RMIX_MIC1_BOOST_OFF, 1, 0),
	SOC_DAPM_DOUBLE("Mic2 Playback Switch",
			AC100_OUT_MXR_SRC,
			AC100_OUT_MXR_SRC_LMIX_MIC2_BOOST_OFF,
			AC100_OUT_MXR_SRC_RMIX_MIC2_BOOST_OFF, 1, 0),
	SOC_DAPM_DOUBLE("Line In Differential Playback Switch",
			AC100_OUT_MXR_SRC,
			AC100_OUT_MXR_SRC_LMIX_LINEINL_LINEINR_OFF,
			AC100_OUT_MXR_SRC_RMIX_LINEINL_LINEINR_OFF, 1, 0),
	SOC_DAPM_DOUBLE("Line In Playback Switch",
			AC100_OUT_MXR_SRC,
			AC100_OUT_MXR_SRC_LMIX_LINEINL_OFF,
			AC100_OUT_MXR_SRC_RMIX_LINEINR_OFF, 1, 0),
	SOC_DAPM_DOUBLE("Aux In Playback Switch",
			AC100_OUT_MXR_SRC,
			AC100_OUT_MXR_SRC_LMIX_AUXINL_OFF,
			AC100_OUT_MXR_SRC_RMIX_AUXINR_OFF, 1, 0),
	SOC_DAPM_DOUBLE("DAC Playback Switch",
			AC100_OUT_MXR_SRC,
			AC100_OUT_MXR_SRC_LMIX_DACL_OFF,
			AC100_OUT_MXR_SRC_RMIX_DACR_OFF, 1, 0),
	SOC_DAPM_DOUBLE("DAC Reversed Playback Switch",
			AC100_OUT_MXR_SRC,
			AC100_OUT_MXR_SRC_LMIX_DACR_OFF,
			AC100_OUT_MXR_SRC_RMIX_DACL_OFF, 1, 0),
};

static const DECLARE_TLV_DB_SCALE(ac100_codec_out_mixer_pregain_scale,
				  -450, 150, 0);

static const DECLARE_TLV_DB_RANGE(ac100_codec_mic_gain_scale,
	0, 0, TLV_DB_SCALE_ITEM(0, 0, 0),
	1, 7, TLV_DB_SCALE_ITEM(3000, 300, 0),
);

static const DECLARE_TLV_DB_SCALE(ac100_codec_pre_gain_scale,
				  -1200, 300, 0);

static const DECLARE_TLV_DB_RANGE(ac100_codec_earpiece_vol_scale,
	0, 1, TLV_DB_SCALE_ITEM(TLV_DB_GAIN_MUTE, 0, 1),
	2, 31, TLV_DB_SCALE_ITEM(-4350, 150, 0),
);

static const DECLARE_TLV_DB_SCALE(ac100_codec_lineout_vol_scale, -450, 150, 0);

static const DECLARE_TLV_DB_SCALE(ac100_codec_hp_vol_scale, -6300, 100, 1);

static const char *ac100_codec_hp_pa_delay_texts[] = {
	"4ms", "8ms", "16ms", "32ms"
};

static SOC_ENUM_SINGLE_DECL(ac100_codec_hp_pa_delay_enum,
			    AC100_HPOUT_CTRL,
			    AC100_HPOUT_CTRL_STARTUP_DELAY_OFF,
			    ac100_codec_hp_pa_delay_texts);

static const char *ac100_codec_hp_pa_cur_texts[] = {
	"low", "mid", "higher", "highest"
};

static SOC_ENUM_SINGLE_DECL(ac100_codec_hp_pa_cur_enum,
			    AC100_HPOUT_CTRL,
			    AC100_HPOUT_CTRL_OUTPUT_CURRENT_OFF,
			    ac100_codec_hp_pa_cur_texts);

static SOC_ENUM_SINGLE_DECL(ac100_codec_ep_pa_cur_enum,
			    AC100_ERPOUT_CTRL,
			    AC100_ERPOUT_CTRL_OUT_CURRENT_OFF,
			    ac100_codec_hp_pa_cur_texts);

static const char *ac100_codec_ep_pa_ramp_time_texts[] = {
	"256ms", "512ms", "640ms", "768ms"
};

static SOC_ENUM_SINGLE_DECL(ac100_codec_ep_pa_ramp_time_enum,
			    AC100_ERPOUT_CTRL,
			    AC100_ERPOUT_CTRL_RAMP_TIME_OFF,
			    ac100_codec_ep_pa_ramp_time_texts);

static const char *ac100_codec_mic_bv_texts[] = {
	"1.88V", "2.09V", "2.33V", "2.5V"
};

static SOC_ENUM_SINGLE_DECL(ac100_codec_mic1_bv_enum,
			    AC100_OUT_MXR_SRC_BST,
			    AC100_OUT_MXR_SRC_BST_MMICBIAS_VOLTAGE_OFF,
			    ac100_codec_mic_bv_texts);

static SOC_ENUM_SINGLE_DECL(ac100_codec_mic2_bv_enum,
			    AC100_OUT_MXR_SRC_BST,
			    AC100_OUT_MXR_SRC_BST_HMICBIAS_VOLTAGE_OFF,
			    ac100_codec_mic_bv_texts);

/* volume / mute controls */
static const struct snd_kcontrol_new ac100_codec_controls[] = {
	/* Microphone Amp boost gain */
	SOC_SINGLE_TLV("Mic1 Boost Volume", AC100_ADC_SRC_BST_CTRL,
		       AC100_ADC_SRC_BST_CTRL_MIC1BOOST_OFF, 0x7, 0,
		       ac100_codec_mic_gain_scale),

	SOC_SINGLE_TLV("Mic2 Boost Volume", AC100_ADC_SRC_BST_CTRL,
		       AC100_ADC_SRC_BST_CTRL_MIC2BOOST_OFF, 0x7, 0,
		       ac100_codec_mic_gain_scale),

	SOC_SINGLE_TLV("Line In Pre-Gain Volume", AC100_ADC_SRC_BST_CTRL,
		       AC100_ADC_SRC_BST_CTRL_LINEIN_DIFF_PREG_OFF, 0x7, 0,
		       ac100_codec_pre_gain_scale),

	SOC_SINGLE_TLV("Aux In Pre-Gain Volume", AC100_ADC_SRC_BST_CTRL,
		       AC100_ADC_SRC_BST_CTRL_AXI_PREG_OFF, 0x7, 0,
		       ac100_codec_pre_gain_scale),

	/* ADC */
	SOC_DOUBLE_TLV("ADC Gain Capture Volume", AC100_ADC_APC_CTRL,
		       AC100_ADC_APC_CTRL_ADCL_GAIN_OFF,
		       AC100_ADC_APC_CTRL_ADCR_GAIN_OFF, 0x7, 0,
		       ac100_codec_out_mixer_pregain_scale),

	/* Mixer pre-gain */
	SOC_SINGLE_TLV("Mic1 Playback Volume", AC100_OUT_MXR_SRC_BST,
		       AC100_OUT_MXR_SRC_BST_MIC1_GAIN_OFF,
		       0x7, 0, ac100_codec_out_mixer_pregain_scale),

	SOC_SINGLE_TLV("Mic2 Playback Volume", AC100_OUT_MXR_SRC_BST,
		       AC100_OUT_MXR_SRC_BST_MIC2_GAIN_OFF,
		       0x7, 0, ac100_codec_out_mixer_pregain_scale),

	SOC_SINGLE_TLV("Line In Playback Volume", AC100_OUT_MXR_SRC_BST,
		       AC100_OUT_MXR_SRC_BST_LINEIN_GAIN_OFF,
		       0x7, 0, ac100_codec_out_mixer_pregain_scale),

	SOC_SINGLE_TLV("Aux In Playback Volume", AC100_OUT_MXR_SRC_BST,
		       AC100_OUT_MXR_SRC_BST_AX_GAIN_OFF,
		       0x7, 0, ac100_codec_out_mixer_pregain_scale),

	SOC_SINGLE_TLV("Headphone Playback Volume",
		       AC100_HPOUT_CTRL,
		       AC100_HPOUT_CTRL_VOLUME_OFF, 0x3f, 0,
		       ac100_codec_hp_vol_scale),

	SOC_SINGLE_TLV("Earpiece Playback Volume",
		       AC100_ERPOUT_CTRL,
		       AC100_ERPOUT_CTRL_VOLUME_OFF, 0x1f, 0,
		       ac100_codec_earpiece_vol_scale),

	SOC_SINGLE_TLV("Speaker Playback Volume",
		       AC100_SPKOUT_CTRL,
		       AC100_SPKOUT_CTRL_VOLUME_OFF, 0x1f, 0,
		       ac100_codec_earpiece_vol_scale),

	SOC_SINGLE_TLV("Line Out Playback Volume",
		       AC100_LINEOUT_CTRL,
		       AC100_LINEOUT_CTRL_LINEOUT_GAIN_OFF, 0x7, 0,
		       ac100_codec_lineout_vol_scale),

	SOC_ENUM("Headphone Amplifier Startup Delay",
		 ac100_codec_hp_pa_delay_enum),
	SOC_ENUM("Headphone Amplifier Current", ac100_codec_hp_pa_cur_enum),

	SOC_ENUM("Earpiece Amplifier Ramp Time",
		 ac100_codec_ep_pa_ramp_time_enum),
	SOC_ENUM("Earpiece Amplifier Current", ac100_codec_ep_pa_cur_enum),

	SOC_ENUM("Mic1 Bias Voltage", ac100_codec_mic1_bv_enum),
	SOC_ENUM("Mic2 Bias Voltage", ac100_codec_mic2_bv_enum),
};

/* Headphone */

static const char * const ac100_codec_hp_src_enum_text[] = {
	"DAC", "Mixer",
};

static SOC_ENUM_DOUBLE_DECL(ac100_codec_hp_src_enum,
			    AC100_HPOUT_CTRL,
			    AC100_HPOUT_CTRL_LEFT_SRC_OFF,
			    AC100_HPOUT_CTRL_RIGHT_SRC_OFF,
			    ac100_codec_hp_src_enum_text);

static const struct snd_kcontrol_new ac100_codec_hp_src[] = {
	SOC_DAPM_ENUM("Headphone Source Playback Route",
		      ac100_codec_hp_src_enum),
};

static const struct snd_kcontrol_new ac100_codec_hp_switch =
	SOC_DAPM_DOUBLE("Headphone Playback Switch",
			AC100_HPOUT_CTRL,
			AC100_HPOUT_CTRL_LEFT_PA_MUTE_OFF,
			AC100_HPOUT_CTRL_RIGHT_PA_MUTE_OFF, 1, 0);

/* Earpiece */

static const struct snd_kcontrol_new ac100_codec_earpiece_switch =
	SOC_DAPM_SINGLE("Playback Switch",
			AC100_ERPOUT_CTRL,
			AC100_ERPOUT_CTRL_MUTE_OFF, 1, 0);

static const char * const ac100_codec_earpiece_src_enum_text[] = {
	"DACR", "DACL", "Right Mixer", "Left Mixer",
};

static SOC_ENUM_SINGLE_DECL(ac100_codec_earpiece_src_enum,
			    AC100_ERPOUT_CTRL,
			    AC100_ERPOUT_CTRL_INPUT_SOURCE_OFF,
			    ac100_codec_earpiece_src_enum_text);

static const struct snd_kcontrol_new ac100_codec_earpiece_src[] = {
	SOC_DAPM_ENUM("Earpiece Source Playback Route",
		      ac100_codec_earpiece_src_enum),
};

/* Speaker */

static const char * const ac100_codec_spk_src_enum_text[] = {
	"Stereo", "Mono",
};

static SOC_ENUM_DOUBLE_DECL(ac100_codec_spk_src_enum,
			    AC100_SPKOUT_CTRL,
			    AC100_SPKOUT_CTRL_LEFT_SRC_OFF,
			    AC100_SPKOUT_CTRL_RIGHT_SRC_OFF,
			    ac100_codec_spk_src_enum_text);

static const struct snd_kcontrol_new ac100_codec_spk_src[] = {
	SOC_DAPM_ENUM("Speaker Source Playback Route",
		      ac100_codec_spk_src_enum),
};

static const struct snd_kcontrol_new ac100_codec_spk_switch =
	SOC_DAPM_DOUBLE("Speaker Playback Switch",
			AC100_SPKOUT_CTRL,
			AC100_SPKOUT_CTRL_LEFT_EN_OFF,
			AC100_SPKOUT_CTRL_RIGHT_EN_OFF, 1, 0);

static const struct snd_kcontrol_new ac100_codec_spk_inv_switch =
	SOC_DAPM_DOUBLE("Speaker Invert Switch",
			AC100_SPKOUT_CTRL,
			AC100_SPKOUT_CTRL_LEFT_INV_EN_OFF,
			AC100_SPKOUT_CTRL_RIGHT_INV_EN_OFF, 1, 0);

/* Line Out */

static const struct snd_kcontrol_new ac100_codec_lineout_mixer_controls[] = {
	SOC_DAPM_SINGLE("Mic1 Playback Switch",
			AC100_LINEOUT_CTRL,
			AC100_LINEOUT_CTRL_LINEOUT_S0_OFF, 1, 0),
	SOC_DAPM_SINGLE("Mic2 Playback Switch",
			AC100_LINEOUT_CTRL,
			AC100_LINEOUT_CTRL_LINEOUT_S1_OFF, 1, 0),
	SOC_DAPM_SINGLE("Right Mixer Playback Switch",
			AC100_LINEOUT_CTRL,
			AC100_LINEOUT_CTRL_LINEOUT_S2_OFF, 1, 0),
	SOC_DAPM_SINGLE("Left Mixer Playback Switch",
			AC100_LINEOUT_CTRL,
			AC100_LINEOUT_CTRL_LINEOUT_S3_OFF, 1, 0),
};

static const struct snd_kcontrol_new ac100_codec_lineout_switch =
	SOC_DAPM_SINGLE("Playback Switch",
			AC100_LINEOUT_CTRL,
			AC100_LINEOUT_CTRL_LINEOUT_EN_OFF, 1, 0);

/* Mic2 Boost Source */

static const char * const ac100_codec_mic2boost_src_enum_text[] = {
	"Mic2", "Mic3",
};

static SOC_ENUM_SINGLE_DECL(ac100_codec_mic2boost_src_enum,
			    AC100_ADC_SRC_BST_CTRL,
			    AC100_ADC_SRC_BST_CTRL_MIC2SLT_OFF,
			    ac100_codec_mic2boost_src_enum_text);

static const struct snd_kcontrol_new ac100_codec_mic2boost_src[] = {
	SOC_DAPM_ENUM("Mic2 Source Capture Route",
		      ac100_codec_mic2boost_src_enum),
};

/* This is done to remove the headphone buffer DC offset. */
static int ac100_codec_hp_power(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	unsigned int val = SND_SOC_DAPM_EVENT_ON(event) ? 0xf : 0;

	// zero cross detection
	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_component_update_bits(component,
					      AC100_ADDA_TUNE1,
					      AC100_ADDA_TUNE1_ZERO_CROSSOVER_EN_MASK,
					      AC100_ADDA_TUNE1_ZERO_CROSSOVER_EN_EN);
	} else {
		snd_soc_component_update_bits(component,
					      AC100_ADDA_TUNE1,
					      AC100_ADDA_TUNE1_ZERO_CROSSOVER_EN_MASK,
					      AC100_ADDA_TUNE1_ZERO_CROSSOVER_EN_DIS);
	}

	snd_soc_component_update_bits(component, AC100_OUT_MXR_DAC_A_CTRL,
				      AC100_OUT_MXR_DAC_A_CTRL_HP_DCRM_EN(0xf),
				      AC100_OUT_MXR_DAC_A_CTRL_HP_DCRM_EN(val));
	return 0;
}

static const struct snd_soc_dapm_widget ac100_codec_widgets[] = {
	/* DAC */
	SND_SOC_DAPM_DAC("Left DAC", NULL, AC100_OUT_MXR_DAC_A_CTRL,
			 AC100_OUT_MXR_DAC_A_CTRL_DAC_AL_EN_OFF, 0),
	SND_SOC_DAPM_DAC("Right DAC", NULL, AC100_OUT_MXR_DAC_A_CTRL,
			 AC100_OUT_MXR_DAC_A_CTRL_DAC_AR_EN_OFF, 0),

	/* ADC */
	SND_SOC_DAPM_ADC("Left ADC", NULL, AC100_ADC_APC_CTRL,
			 AC100_ADC_APC_CTRL_ADCL_EN_OFF, 0),
	SND_SOC_DAPM_ADC("Right ADC", NULL, AC100_ADC_APC_CTRL,
			 AC100_ADC_APC_CTRL_ADCR_EN_OFF, 0),

	/*
	 * Due to this component and the codec belonging to separate DAPM
	 * contexts, we need to manually link the above widgets to their
	 * stream widgets at the card level.
	 */

        /* Headphones */

	SND_SOC_DAPM_REGULATOR_SUPPLY("cpvdd", 0, 0),
	SND_SOC_DAPM_MUX("Left Headphone Source",
			 SND_SOC_NOPM, 0, 0, ac100_codec_hp_src),
	SND_SOC_DAPM_MUX("Right Headphone Source",
			 SND_SOC_NOPM, 0, 0, ac100_codec_hp_src),
	SND_SOC_DAPM_SWITCH("Left Headphone Switch",
			    SND_SOC_NOPM, 0, 0, &ac100_codec_hp_switch),
	SND_SOC_DAPM_SWITCH("Right Headphone Switch",
			    SND_SOC_NOPM, 0, 0, &ac100_codec_hp_switch),
	SND_SOC_DAPM_OUT_DRV("Left Headphone Amp",
			     SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("Right Headphone Amp",
			     SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("Headphone Amp", AC100_HPOUT_CTRL,
			    AC100_HPOUT_CTRL_PA_EN_OFF, 0,
			    ac100_codec_hp_power,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_OUTPUT("HP"),

        /* Earpiece */

	SND_SOC_DAPM_MUX("Earpiece Source Playback Route",
			 SND_SOC_NOPM, 0, 0, ac100_codec_earpiece_src),
	SND_SOC_DAPM_SWITCH("Earpiece",
			    SND_SOC_NOPM, 0, 0, &ac100_codec_earpiece_switch),
	SND_SOC_DAPM_OUT_DRV("Earpiece Amp", AC100_ERPOUT_CTRL,
			     AC100_ERPOUT_CTRL_PA_EN_OFF, 0, NULL, 0),
	SND_SOC_DAPM_OUTPUT("EARPIECE"),

	/* Speaker */

	SND_SOC_DAPM_MUX("Left Speaker Source",
			 SND_SOC_NOPM, 0, 0, ac100_codec_spk_src),
	SND_SOC_DAPM_MUX("Right Speaker Source",
			 SND_SOC_NOPM, 0, 0, ac100_codec_spk_src),
	SND_SOC_DAPM_SWITCH("Left Speaker Switch",
			    SND_SOC_NOPM, 0, 0, &ac100_codec_spk_switch),
	SND_SOC_DAPM_SWITCH("Right Speaker Switch",
			    SND_SOC_NOPM, 0, 0, &ac100_codec_spk_switch),
	SND_SOC_DAPM_SWITCH("Left Speaker Invert Switch",
			    SND_SOC_NOPM, 0, 0, &ac100_codec_spk_inv_switch),
	SND_SOC_DAPM_SWITCH("Right Speaker Invert Switch",
			    SND_SOC_NOPM, 0, 0, &ac100_codec_spk_inv_switch),
	SND_SOC_DAPM_OUTPUT("SPKOUTL"),
	SND_SOC_DAPM_OUTPUT("SPKOUTR"),

	/* Line Out */

	SND_SOC_DAPM_MIXER("Line Out Mixer", SND_SOC_NOPM, 0, 0,
			   ac100_codec_lineout_mixer_controls,
			   ARRAY_SIZE(ac100_codec_lineout_mixer_controls)),
	SND_SOC_DAPM_SWITCH("Line Out",
			    SND_SOC_NOPM, 0, 0, &ac100_codec_lineout_switch),
	SND_SOC_DAPM_OUTPUT("LINEOUT"),

	/* Microphone 1 */

	SND_SOC_DAPM_INPUT("MIC1"),
	SND_SOC_DAPM_SUPPLY("MBIAS", AC100_ADC_APC_CTRL,
			    AC100_ADC_APC_CTRL_MBIAS_EN_OFF,
			    0, NULL, 0),
	SND_SOC_DAPM_PGA("Mic1 Amplifier", AC100_ADC_SRC_BST_CTRL,
			 AC100_ADC_SRC_BST_CTRL_MIC1AMPEN_OFF, 0, NULL, 0),

        /* Microphone 2 and 3 */

	SND_SOC_DAPM_INPUT("MIC2"),
	SND_SOC_DAPM_INPUT("MIC3"),
	SND_SOC_DAPM_MUX("Mic2 Amplifier Source",
			 SND_SOC_NOPM, 0, 0, ac100_codec_mic2boost_src),
	SND_SOC_DAPM_SUPPLY("HBIAS", AC100_ADC_APC_CTRL,
			    AC100_ADC_APC_CTRL_HBIAS_EN_OFF,
			    0, NULL, 0),
	SND_SOC_DAPM_PGA("Mic2 Amplifier", AC100_ADC_SRC_BST_CTRL,
			 AC100_ADC_SRC_BST_CTRL_MIC2AMPEN_OFF, 0, NULL, 0),

	/* Line input */

	SND_SOC_DAPM_INPUT("LINEIN"),

	/* Aux input */

	SND_SOC_DAPM_INPUT("AUXIN"),

	/* Output mixers */
	SND_SOC_DAPM_MIXER("Left Mixer", AC100_OUT_MXR_DAC_A_CTRL,
			   AC100_OUT_MXR_DAC_A_CTRL_AL_MIX_EN_OFF, 0,
			   ac100_codec_mixer_controls,
			   ARRAY_SIZE(ac100_codec_mixer_controls)),
	SND_SOC_DAPM_MIXER("Right Mixer", AC100_OUT_MXR_DAC_A_CTRL,
			   AC100_OUT_MXR_DAC_A_CTRL_AR_MIX_EN_OFF, 0,
			   ac100_codec_mixer_controls,
			   ARRAY_SIZE(ac100_codec_mixer_controls)),

	/* Input mixers */
	SND_SOC_DAPM_MIXER("Left ADC Mixer", SND_SOC_NOPM, 0, 0,
			   ac100_codec_adc_mixer_controls,
			   ARRAY_SIZE(ac100_codec_adc_mixer_controls)),
	SND_SOC_DAPM_MIXER("Right ADC Mixer", SND_SOC_NOPM, 0, 0,
			   ac100_codec_adc_mixer_controls,
			   ARRAY_SIZE(ac100_codec_adc_mixer_controls)),
};

static const struct snd_soc_dapm_route ac100_codec_routes[] = {
	/* Microphone Routes */
	{ "Mic1 Amplifier", NULL, "MIC1"},
	{ "Mic2 Amplifier", NULL, "Mic2 Amplifier Source"},
	{ "Mic2 Amplifier Source", "Mic2", "MIC2" },
	{ "Mic2 Amplifier Source", "Mic3", "MIC3" },

	/* Mixer Routes */
	{ "Left Mixer", "Mic1 Playback Switch", "Mic1 Amplifier" },
	{ "Left Mixer", "Mic2 Playback Switch", "Mic2 Amplifier" },
	{ "Left Mixer", "Line In Differential Playback Switch", "LINEIN" },
	{ "Left Mixer", "Line In Playback Switch", "LINEIN" },
	{ "Left Mixer", "Aux In Playback Switch", "AUXIN" },
	{ "Left Mixer", "DAC Playback Switch", "Left DAC" },
	{ "Left Mixer", "DAC Reversed Playback Switch", "Right DAC" },

	{ "Right Mixer", "Mic1 Playback Switch", "Mic1 Amplifier" },
	{ "Right Mixer", "Mic2 Playback Switch", "Mic2 Amplifier" },
	{ "Right Mixer", "Line In Differential Playback Switch", "LINEIN" },
	{ "Right Mixer", "Line In Playback Switch", "LINEIN" },
	{ "Right Mixer", "Aux In Playback Switch", "AUXIN" },
	{ "Right Mixer", "DAC Playback Switch", "Right DAC" },
	{ "Right Mixer", "DAC Reversed Playback Switch", "Left DAC" },

	/* ADC Mixer Routes */
	{ "Left ADC Mixer", "Mic1 Capture Switch", "Mic1 Amplifier" },
	{ "Left ADC Mixer", "Mic2 Capture Switch", "Mic2 Amplifier" },
	{ "Left ADC Mixer", "Line In Differential Capture Switch", "LINEIN" },
	{ "Left ADC Mixer", "Line In Capture Switch", "LINEIN" },
	{ "Left ADC Mixer", "Aux In Capture Switch", "AUXIN" },
	{ "Left ADC Mixer", "Mixer Capture Switch", "Left Mixer" },
	{ "Left ADC Mixer", "Mixer Reversed Capture Switch", "Right Mixer" },

	{ "Right ADC Mixer", "Mic1 Capture Switch", "Mic1 Amplifier" },
	{ "Right ADC Mixer", "Mic2 Capture Switch", "Mic2 Amplifier" },
	{ "Right ADC Mixer", "Line In Differential Capture Switch", "LINEIN" },
	{ "Right ADC Mixer", "Line In Capture Switch", "LINEIN" },
	{ "Right ADC Mixer", "Aux In Capture Switch", "AUXIN" },
	{ "Right ADC Mixer", "Mixer Capture Switch", "Right Mixer" },
	{ "Right ADC Mixer", "Mixer Reversed Capture Switch", "Left Mixer" },

	/* ADC Routes */
	{ "Left ADC", NULL, "Left ADC Mixer" },
	{ "Right ADC", NULL, "Right ADC Mixer" },

	/* Headphone Routes */
	{ "Left Headphone Source", "DAC", "Left DAC" },
	{ "Left Headphone Source", "Mixer", "Left Mixer" },
	{ "Left Headphone Switch", "Headphone Playback Switch", "Left Headphone Source" },
	{ "Left Headphone Amp", NULL, "Left Headphone Switch" },
	{ "Left Headphone Amp", NULL, "Headphone Amp" },
	{ "HP", NULL, "Left Headphone Amp" },

	{ "Right Headphone Source", "DAC", "Right DAC" },
	{ "Right Headphone Source", "Mixer", "Right Mixer" },
	{ "Right Headphone Switch", "Headphone Playback Switch", "Right Headphone Source" },
	{ "Right Headphone Amp", NULL, "Right Headphone Switch" },
	{ "Right Headphone Amp", NULL, "Headphone Amp" },
	{ "HP", NULL, "Right Headphone Amp" },

	{ "Headphone Amp", NULL, "cpvdd" },

	/* Speaker Routes */
	{ "Left Speaker Source", "Stereo", "Left Mixer" },
	{ "Left Speaker Source", "Mono", "Right Mixer" },
	{ "Left Speaker Source", "Mono", "Left Mixer" },
	{ "Left Speaker Switch", "Speaker Playback Switch", "Left Speaker Source" },
	{ "SPKOUTL", NULL, "Left Speaker Switch" },

	{ "Right Speaker Source", "Stereo", "Right Mixer" },
	{ "Right Speaker Source", "Mono", "Right Mixer" },
	{ "Right Speaker Source", "Mono", "Left Mixer" },
	{ "Right Speaker Switch", "Speaker Playback Switch", "Right Speaker Source" },
	{ "SPKOUTR", NULL, "Right Speaker Switch" },

	/* Earpiece Routes */
	{ "Earpiece Source Playback Route", "DACR", "Right DAC" },
	{ "Earpiece Source Playback Route", "DACL", "Left DAC" },
	{ "Earpiece Source Playback Route", "Right Mixer", "Right Mixer" },
	{ "Earpiece Source Playback Route", "Left Mixer", "Left Mixer" },
	{ "Earpiece", "Playback Switch", "Earpiece Source Playback Route" },
	{ "Earpiece Amp", NULL, "Earpiece" },
	{ "EARPIECE", NULL, "Earpiece Amp" },

	/* Line-out Routes */
	{ "Line Out", "Playback Switch", "Line Out Mixer" },
	{ "Line Out Mixer", "Mic1 Playback Switch", "Mic1 Amplifier" },
	{ "Line Out Mixer", "Mic2 Playback Switch", "Mic2 Amplifier" },
	{ "Line Out Mixer", "Right Mixer Playback Switch", "Right Mixer" },
	{ "Line Out Mixer", "Left Mixer Playback Switch", "Left Mixer" },
	{ "LINEOUT", NULL, "Line Out" },
};

static int ac100_codec_set_bias_level(struct snd_soc_component *component,
				      enum snd_soc_bias_level level)
{
	if (level == SND_SOC_BIAS_OFF) {
		pr_err("XX: idle bias off\n");
	}

	return 0;
}

static const struct snd_soc_component_driver ac100_codec_analog_cmpnt_drv = {
	.controls		= ac100_codec_controls,
	.num_controls		= ARRAY_SIZE(ac100_codec_controls),
	.dapm_widgets		= ac100_codec_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(ac100_codec_widgets),
	.dapm_routes		= ac100_codec_routes,
	.num_dapm_routes	= ARRAY_SIZE(ac100_codec_routes),
	.set_bias_level		= ac100_codec_set_bias_level,
};

static int ac100_codec_probe(struct platform_device *pdev)
{
	struct ac100_dev *ac100 = dev_get_drvdata(pdev->dev.parent);
	struct ac100_codec *codec;
	int ret = 0;

	codec = devm_kzalloc(&pdev->dev, sizeof(*codec), GFP_KERNEL);
	if (!codec)
		return -ENOMEM;

	codec->dev = &pdev->dev;
	platform_set_drvdata(pdev, codec);

	snd_soc_component_init_regmap(&codec->component, ac100->regmap);

	ret = snd_soc_component_initialize(&codec->component,
					   &ac100_codec_analog_cmpnt_drv,
					   &pdev->dev);
	if (ret < 0)
		return ret;

	ret = snd_soc_add_component(&codec->component, NULL, 0);
	if (ret) {
		dev_err(&pdev->dev,
			"Failed to register codec component (%d)\n", ret);
		return ret;
	}

	return ret;
}

static int ac100_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);

	/*
	 * We do not call snd_soc_component_exit_regmap, because regmap
	 * is still owned by the mfd device.
	 */
	return 0;
}

static const struct of_device_id ac100_codec_of_match[] = {
	{ .compatible = "x-powers,ac100-codec-analog" },
	{}
};
MODULE_DEVICE_TABLE(of, ac100_codec_of_match);

static struct platform_driver ac100_codec_driver = {
	.driver = {
		.name = "ac100-codec-analog",
		.of_match_table = ac100_codec_of_match,
	},
	.probe = ac100_codec_probe,
	.remove = ac100_codec_remove,
};
module_platform_driver(ac100_codec_driver);

MODULE_DESCRIPTION("X-Powers AC100 codec driver");
MODULE_AUTHOR("Ondrej Jirman <megi@xff.cz>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ac100-codec");
