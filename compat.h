/**
 * This file is part of samsung-ril.
 *
 * Copyright (C) 2010-2011 Joerie de Gram <j.de.gram@gmail.com>
 * Copyright (C) 2011 Paul Kocialkowski <contact@oaulk.fr>
 * Copyright (C) 2012 Alexander Tarasikov <alexander.tarasikov@gmail.com>
 *
 * samsung-ril is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * samsung-ril is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with samsung-ril.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __COMPAT_H__
#define __COMPAT_H__

#include <telephony/ril.h>
#include <utils/Log.h>

#ifndef LOGE
	#define LOGE ALOGE
#endif

#ifndef LOGI
	#define LOGI ALOGI
#endif

#ifndef LOGD
	#define LOGD ALOGD
#endif

#if RIL_VERSION >= 6
	#define RIL_REQUEST_REGISTRATION_STATE RIL_REQUEST_VOICE_REGISTRATION_STATE
	#define RIL_REQUEST_GPRS_REGISTRATION_STATE RIL_REQUEST_DATA_REGISTRATION_STATE
	#define RIL_SignalStrength RIL_SignalStrength_v6
	#define RIL_CardStatus RIL_CardStatus_v6
	#define RIL_SIM_IO RIL_SIM_IO_v6
	#define RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED
	#define RIL_LastDataCallActivateFailCause RIL_DataCallFailCause
	#define RIL_Data_Call_Response RIL_Data_Call_Response_v6
	#define COMPAT_RADIO_STATE_ON RADIO_STATE_ON
#else
	#define COMPAT_RADIO_STATE_ON RADIO_STATE_SIM_READY
#endif

//set it to the maximum supported revision
//we've not yet fully implemented version 7
#if RIL_VERSION >= 6
	#define SAMSUNG_RIL_VERSION 6
#else
	#define SAMSUNG_RIL_VERSION RIL_VERSION
#endif

#endif //__COMPAT_H__
