#include <stdio.h>
#include <string.h>

#define LOG_TAG "RIL-UTIL"
#include <utils/Log.h>

/**
 * Converts a hexidecimal string to binary
 */
void hex2bin(const char *data, int length, unsigned char *buf)
{
	int i = 0;
	char b = 0;
	unsigned char *p = buf;

	length ^= 0x01;

	while(i < length) {
		if(data[i] - '0' < 10)
			b = data[i++] - '0';
		else if(data[i] - 'a' < 7)
			b = data[i++] - 'a' + 10;
		else if(data[i] - 'A' < 7)
			b = data[i++] - 'A' + 10;

		b = (b << 4);

		if(data[i] - '0' < 10)
			b |= data[i++] - '0';
		else if(data[i] - 'a' < 7)
			b |= data[i++] - 'a' + 10;
		else if(data[i] - 'A' < 7)
			b |= data[i++] - 'A' + 10;

		*p++ = b;
	}
}

/**
 * Converts binary data to a hexidecimal string
 */
void bin2hex(const unsigned char *data, int length, char *buf)
{
	int i;
	char b;
	char *p = buf;

	for(i = 0; i < length; i++) {
		b = (data[i] >> 4 & 0x0f);
		b += (b < 10) ? '0' : ('a' - 10);
		*p++ = b;

		b = (data[i] & 0x0f);
		b += (b < 10) ? '0' : ('a' - 10);
		*p++ = b;
	}

	*p = '\0';
}

/**
 * Converts IPC network registration status to Android RIL format
 */
unsigned char registatus_ipc2ril(unsigned char status)
{
	switch(status) {
		case 1:
			return 0;
		case 2:
			return 1;
		case 3:
			return 2;
		case 4:
			return 13;
		case 5:
			return 14;
		case 6:
			return 5;
		default:
			LOGE("%s: invalid status %d", __FUNCTION__, status);
			return 255;
	}
}

/**
 * Converts IPC network access technology to Android RIL format
 */
unsigned char act_ipc2ril(unsigned char act)
{
	switch(act) {
		case 1:
		case 2:
			return 1;
		case 3:
			return 2;
		case 4:
			return 3;
		default:
			return 0;
	}
}

/**
 * Converts IPC preferred network type to Android RIL format
 */
unsigned char modesel_ipc2ril(unsigned char mode)
{
	switch(mode) {
		case 0:
			return 7;
		case 1:
		case 3:
			return 1;
		case 2:
		case 4:
			return 2;
		default:
			return 255;
	}
}

/**
 * Converts Android RIL preferred network type to IPC format
 */
unsigned char modesel_ril2ipc(unsigned char mode)
{
	switch(mode) {
		case 1:
			return 2;
		case 2:
			return 3;
		default:
			return 1;
	}
}

