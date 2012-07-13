/**
 * This file is part of samsung-ril.
 *
 * Copyright (C) 2010-2011 Joerie de Gram <j.de.gram@gmail.com>
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

#ifndef _SAMSUNG_RIL_UTIL_H_
#define _SAMSUNG_RIL_UTIL_H_

void bin2hex(const unsigned char *data, int length, char *buf);
void hex2bin(const char *data, int length, unsigned char *buf);
int gsm72ascii(unsigned char *data, char **data_dec, int length);
int ascii2gsm7(char *data, unsigned char **data_enc, int length);
void hex_dump(void *data, int size);
int utf8_write(char *utf8, int offset, int v);

typedef enum {
	SMS_CODING_SCHEME_UNKNOWN = 0,
	SMS_CODING_SCHEME_GSM7,
	SMS_CODING_SCHEME_UCS2
} SmsCodingScheme;

SmsCodingScheme sms_get_coding_scheme(int dataCoding);

#endif
