/*
 * Copyright (C) 2008 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ASM_ARCH_SEC_HEADSET_H
#define __ASM_ARCH_SEC_HEADSET_H

#ifdef __KERNEL__

enum {
	SEC_JACK_NO_DEVICE		= 0x0,
	SEC_HEADSET_4_POLE_DEVICE	= 0x01 << 0,
	SEC_HEADSET_3_POLE_DEVICE	= 0x01 << 1,
	SEC_TTY_DEVICE			= 0x01 << 2,
	SEC_FM_HEADSET			= 0x01 << 3,
	SEC_FM_SPEAKER			= 0x01 << 4,
	SEC_TVOUT_DEVICE		= 0x01 << 5,
	SEC_EXTRA_DOCK_SPEAKER		= 0x01 << 6,
	SEC_EXTRA_CAR_DOCK_SPEAKER	= 0x01 << 7,
	SEC_UNKNOWN_DEVICE		= 0x01 << 8,
};

struct sec_jack_zone {
	unsigned int adc_high;
	unsigned int delay_ms;
	unsigned int check_count;
	unsigned int jack_type;
};

struct sec_jack_platform_data {
	void	(*set_micbias_state) (bool);
	void 	(*set_popup_sw_state) (bool);
	int	(*get_adc_value) (void);
	struct sec_jack_zone	*zones;
	int	num_zones;
	int	det_gpio;
	int	short_send_end_gpio;
        int     open_send_end_gpio;
	bool	det_active_high;
	bool    det_active_low;
	bool	send_end_active_high; 
	int 	open_send_end_eintr;
	int 	short_send_end_eintr; 
};
#endif

#endif
