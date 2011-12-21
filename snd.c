/**
 * This file is part of samsung-ril.
 *
 * Copyright (C) 2010-2011 Joerie de Gram <j.de.gram@gmail.com>
 * Copyright (C) 2011 Paul Kocialkowski <contact@oaulk.fr>
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

#define LOG_TAG "RIL-SND"
#include <utils/Log.h>

#include "samsung-ril.h"
#include "util.h"

void srs_snd_set_call_clock_sync(struct srs_message *message)
{
	unsigned char data = *((unsigned char *) message->data);
	LOGE("SetCallClockSync data is 0x%x\n", data);

	ipc_fmt_send(IPC_SND_CLOCK_CTRL, IPC_TYPE_EXEC, &data, sizeof(data), reqIdNew());
}
