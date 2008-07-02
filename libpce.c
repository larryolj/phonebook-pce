/*
 *
 *  Lib PhoneBook Client Access
 *
 *  Copyright (C) 2008  Larry Junior <larry.junior@openbossa.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libpce.h"

#define XOBEX_BT_PHONEBOOK	"x-bt/phonebook"
#define XOBEX_BT_VCARD		"x-bt/vcard"
#define XOBEX_BT_VCARDLIST	"x-bt/vcard-listing"

#define PBAP_PCE_UUID ((const uint8_t []) \
{ 0x79, 0x61, 0x35, 0xf0, \
		0xf0, 0xc5, 0x11, 0xd8, 0x09, 0x66, \
		0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66 })

static void debug(const char *msg)
{
	printf("[PCE] %s", msg);
}

pce_t *PCE_Connect(const char *bdaddr, uint8_t channel)
{
	obex_t *obex = NULL;
	obex_object_t *obj;
	obex_headerdata_t hd;
	pce_t *pce;
	int fd;

	/* FIXME convert char to bdaddr */
	if (bacmp(bdaddr, BDADDR_ANY) == 0) {
		debug("ERROR Device bt address error!\n");
		goto fail;
	}

	obex = OBEX_Init(OBEX_TRANS_BLUETOOTH, obex_pce_event, 0);
	if (!obex) {
		debug("OBEX_Init failed");
		goto fail;
	}

	if (BtOBEX_TransportConnect(obex, BDADDR_ANY, bdaddr, channel) < 0) {
		debug("Transport connect error!");
		goto fail;
	}

	obj = OBEX_ObjectNew(obex, OBEX_CMD_CONNECT);
	if (!obj) {
		debug("Error creating object");
		goto fail;
	}

	hd.bs = PBAP_PCE_UUID;
	if (OBEX_ObjectAddHeader(obex, obj, OBEX_HDR_TARGET, hd,
			sizeof(PBAP_PCE_UUID), OBEX_FL_FIT_ONE_PACKET) < 0) {
		debug("Error adding header");
		OBEX_ObjectDelete(obex, obj);
		goto fail;
	}

	pce = g_new0(pce_t, 1);
	pce->obex = obex;

	/* FIXME sync request*/

	return pce;

fail:
	if (obex)
		OBEX_Cleanup(obex);
	debug("ERROR Creating connection\n");
	return NULL;
}


int PCE_Set_PB(pce_t pce, char *name)
{
	/* FIXME */
	return 0;
}

int PCE_Pull_PB(pce_t *pce, pce_query_t *query, char **buf)
{
	/* FIXME */
	return 0;
}

int PCE_VCard_List(pce_t *pce, pce_query_t *query, char **buf)
{
	/* FIXME */
	return 0;
}

int PCE_VCard_Entry(pce_t *pce, pce_query_t *query, char **buf)
{
	/* FIXME */
	return 0;
}
