#define LOG_TAG "RIL-DISP"
#include <utils/Log.h>

#include "samsung-ril.h"
#include "util.h"

extern const struct RIL_Env *rilenv;
extern struct radio_state radio;
extern struct ipc_client *ipc_client;

void respondIconSignalStrength(RIL_Token t, void *data, int length)
{
	struct ipc_disp_icon_info *signal_info = (struct ipc_disp_icon_info*)data;
	RIL_SignalStrength ss;
	int rssi;

	/* Don't consider this if modem isn't in normal power mode. */
	if(radio.power_mode < POWER_MODE_NORMAL)
		return;

	memset(&ss, 0, sizeof(ss));

	/* Multiplying the number of bars by 3 yields
	 * an asu with an equal number of bars in Android
	 */
	rssi = (3 * signal_info->rssi);

	ss.GW_SignalStrength.signalStrength = rssi;
	ss.GW_SignalStrength.bitErrorRate = 99;

	/* Send CDMA and EVDO levels even in GSM mode */
	ss.CDMA_SignalStrength.dbm = rssi;
	ss.CDMA_SignalStrength.ecio = 200;

	ss.EVDO_SignalStrength.dbm = rssi;
	ss.EVDO_SignalStrength.ecio = 200;

	LOGD("Signal Strength is %d\n", rssi);

	RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH, &ss, sizeof(ss));
}

void respondSignalStrength(RIL_Token t, void *data, int length)
{
	struct ipc_disp_rssi_info *rssi_info = (struct ipc_disp_rssi_info*)data;
	RIL_SignalStrength ss;
	int rssi;

	/* Don't consider this if modem isn't in normal power mode. */
	if(radio.power_mode < POWER_MODE_NORMAL)
		return;

	memset(&ss, 0, sizeof(ss));

	if(rssi_info->rssi > 0x6f) {
		rssi = 0;
	} else {
		rssi = (((rssi_info->rssi - 0x71) * -1) - ((rssi_info->rssi - 0x71) * -1) % 2) / 2;
		if(rssi > 31)
			rssi = 31;
	}

	/* Send CDMA and EVDO levels even in GSM mode */
	ss.GW_SignalStrength.signalStrength = rssi;
	ss.GW_SignalStrength.bitErrorRate = 99;

	ss.CDMA_SignalStrength.dbm = rssi;
	ss.CDMA_SignalStrength.ecio = 200;

	ss.EVDO_SignalStrength.dbm = rssi;
	ss.EVDO_SignalStrength.ecio = 200;

	LOGD("Signal Strength is %d\n", rssi);

	RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH, &ss, sizeof(ss));
}
