#define LOG_TAG "RIL-PWR"
#include <utils/Log.h>

#include "samsung-ril.h"
#include "util.h"

extern const struct RIL_Env *rilenv;
extern struct radio_state radio;
extern struct ipc_client *ipc_client;

/**
 * Out: RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED
 *   Modem lets us know it's powered on. Though, it's still in LPM and should
 *   be considered as OFF. Send this to update RILJ radio state (OFF)
 */
void respondPowerUp(void)
{
	/* H1 baseband firmware bug workaround: sleep for 25ms to allow for nvram to initialize */
	usleep(25000);

	radio.radio_state = RADIO_STATE_OFF;
	radio.power_mode = POWER_MODE_LPM;
	RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
}

/**
 * In: IPC_PWR_PHONE_STATE
 *   Noti from the modem giving current power mode (LPM or NORMAL)
 *   LPM = Low Power Mode (airplane mode for instance)
 *
 * Out: RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED
 *   Update radio state according to modem power state
 */
void respondPowerPhoneState(struct ipc_message_info *info)
{
	uint8_t state = *((uint8_t *)info->data);

	switch(state)
	{
		/* This shouldn't append for LPM (no respond message) */
		case IPC_PWR_R(IPC_PWR_PHONE_STATE_LPM):
			radio.power_mode = POWER_MODE_LPM;
			radio.radio_state = RADIO_STATE_OFF;
			LOGD("Got power to LPM");
			RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
		break;
		case IPC_PWR_R(IPC_PWR_PHONE_STATE_NORMAL):
			usleep(3000);

			radio.power_mode = POWER_MODE_NORMAL;
			radio.radio_state = RADIO_STATE_SIM_NOT_READY;
			LOGD("Got power to NORMAL");

			/* 
			 * return RIL_E_SUCCESS is done at IPC_SEC_PIN_STATUS:
			 * breaks return-from-airplane-mode if done here 
			 */
			RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
		break;
	}

	RadioTokensCheck();
}

/**
 * In: RIL_REQUEST_RADIO_POWER
 *   Request ON or OFF radio power mode
 *
 * Out: IPC_PWR_PHONE_STATE
 *   Order the modem to get in required power mode
 */
void requestPower(RIL_Token t, void *data, size_t datalen)
{
	int power_state = *((int *)data);
	unsigned short power_data;

	LOGD("requested power_state is %d", power_state);

	if(power_state > 0) {
		LOGD("Request power to NORMAL");
		power_data = IPC_PWR_PHONE_STATE_NORMAL;
		ipc_client_send(ipc_client, IPC_PWR_PHONE_STATE, IPC_TYPE_EXEC, (void *) &power_data, sizeof(power_data), getRequestId(t));

		radio.tokens.radio_power = t;

		/* Don't tell the RIL we're not off anymore: wait for the message */
	} else {
		LOGD("Request power to LPM");
		power_data = IPC_PWR_PHONE_STATE_LPM;
		ipc_client_send(ipc_client, IPC_PWR_PHONE_STATE, IPC_TYPE_EXEC, (void *) &power_data, sizeof(power_data), getRequestId(t));

		RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);

		/* We're not going to get any message to make sure we're in LPM so tell RILJ we're off anyway */
		radio_init_lpm();
		RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
	}
}
