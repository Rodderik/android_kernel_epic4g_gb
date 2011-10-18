/*
 * linux/drivers/power/s3c6410_battery.h
 *
 * Battery measurement code for S3C6410 platform.
 *
 * Copyright (C) 2009 Samsung Electronics.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define DRIVER_NAME	"sec-battery"

/*
 * Battery Table
 */
#define BATT_CAL		2447	/* 3.60V */

#define BATT_MAXIMUM		406	/* 4.176V */
#define BATT_FULL		353	/* 4.10V  */
#define BATT_SAFE_RECHARGE	353	/* 4.10V */
#define BATT_ALMOST_FULL	188	/* 3.8641V */
#define BATT_HIGH		112	/* 3.7554V */
#define BATT_MED		66	/* 3.6907V */
#define BATT_LOW		43	/* 3.6566V */
#define BATT_CRITICAL		8	/* 3.6037V */
#define BATT_MINIMUM		(-28)	/* 3.554V */
#define BATT_OFF		(-128)	/* 3.4029V */
#if defined(CONFIG_MACH_ATLAS)
#define  __VZW_AUTH_CHECK__
#endif
#ifdef CONFIG_MACH_VICTORY
#define SPRINT_SLATE_TEST
#endif
/*
 * ADC channel, please update the ENDOFADC accordingly with the adc_channel_type
 * added in platform data
 */

#define ENDOFADC	9

/* Battery level test */
//#define __SOC_TEST__

#if 0
#ifdef CONFIG_MACH_ATLAS 
enum adc_channel_type {
	// hanapark_Atlas
	S3C_ADC_VOLTAGE = 1,
	S3C_ADC_CHG_CURRENT = 2,
	S3C_ADC_TEMPERATURE = 6,
	ENDOFADC
};
#else
enum adc_channel_type{
        S3C_ADC_TEMPERATURE = 1,
        S3C_ADC_CHG_CURRENT = 2,
        S3C_ADC_V_F = 4,
        S3C_ADC_HW_VERSION = 7,
        S3C_ADC_VOLTAGE = 8,
        ENDOFADC
};
#endif
#endif

enum {
	BATT_VOL = 0,
	BATT_VOL_ADC,
	BATT_TEMP,
	BATT_TEMP_ADC,
	BATT_CHARGING_SOURCE,
	BATT_FG_SOC,
	BATT_RESET_SOC,
	BATT_DECIMAL_POINT_LEVEL,       // lobat pwroff
	CHARGING_MODE_BOOTING,
	BATT_TEMP_CHECK,
	BATT_FULL_CHECK,
#ifdef __VZW_AUTH_CHECK__
        AUTH_BATTERY,
#endif
        BATT_CHG_CURRENT_AVER,
	BATT_TYPE,
#ifdef __SOC_TEST__
	SOC_TEST,
#endif
#ifdef SPRINT_SLATE_TEST
        SLATE_TEST_MODE,
#endif
#ifdef CONFIG_MACH_VICTORY
	BATT_V_F_ADC,
	BATT_USE_CALL,	/* battery use */
	BATT_USE_VIDEO,
	BATT_USE_MUSIC,
	BATT_USE_BROWSER,
	BATT_USE_HOTSPOT,
	BATT_USE_CAMERA,
	BATT_USE_DATA_CALL,
	BATT_USE_WIMAX,
	BATT_USE,	/* flags */
#endif
};

#define TOTAL_CHARGING_TIME	(6*60*60)	/* 6 hours */
#define TOTAL_RECHARGING_TIME	  (2*60*60)	/* 2 hours */

#define COMPENSATE_VIBRATOR		19
#define COMPENSATE_CAMERA		25
#define COMPENSATE_MP3			17
#define COMPENSATE_VIDEO		28
#define COMPENSATE_VOICE_CALL_2G	13
#define COMPENSATE_VOICE_CALL_3G	14
#define COMPENSATE_DATA_CALL		25
#define COMPENSATE_LCD			0
#define COMPENSATE_TA			0
#define COMPENSATE_CAM_FALSH		0
#define COMPENSATE_BOOTING		52

#define SOC_LB_FOR_POWER_OFF		27

#ifdef CONFIG_MACH_VICTORY
#define RECHARGE_COND_VOLTAGE		4110000
#else
#define RECHARGE_COND_VOLTAGE		4130000
#endif
#define RECHARGE_COND_TIME		(30*1000)	/* 30 seconds */
#define LOW_BATT_COND_VOLTAGE           3400
#define LOW_BATT_COND_LEVEL             0

