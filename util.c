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
		b = 0;

		if(data[i] - '0' < 10)
			b = data[i] - '0';
		else if(data[i] - 'a' < 7)
			b = data[i] - 'a' + 10;
		else if(data[i] - 'A' < 7)
			b = data[i] - 'A' + 10;
		i++;

		b = (b << 4);

		if(data[i] - '0' < 10)
			b |= data[i] - '0';
		else if(data[i] - 'a' < 7)
			b |= data[i] - 'a' + 10;
		else if(data[i] - 'A' < 7)
			b |= data[i] - 'A' + 10;
		i++;

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
		b = 0;

		b = (data[i] >> 4 & 0x0f);
		b += (b < 10) ? '0' : ('a' - 10);
		*p++ = b;

		b = (data[i] & 0x0f);
		b += (b < 10) ? '0' : ('a' - 10);
		*p++ = b;
	}

	*p = '\0';
}
