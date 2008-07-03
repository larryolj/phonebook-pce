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

#include <bluetooth/bluetooth.h>
#include "libpce.h"

#define XOBEX_BT_PHONEBOOK	"x-bt/phonebook"
#define XOBEX_BT_VCARD		"x-bt/vcard"
#define XOBEX_BT_VCARDLIST	"x-bt/vcard-listing"

#define PBAP_APP_ORDER_ID			0x01
#define PBAP_APP_SEARCH_VAL_ID		0x02
#define PBAP_APP_SEARCH_ATT_ID		0x03
#define PBAP_APP_MAXLIST_ID			0x04
#define PBAP_APP_LISTOFFSET_ID		0x05
#define PBAP_APP_FILTER_ID			0x06
#define PBAP_APP_FORMAT_ID			0x07
#define PBAP_APP_PBSIZE_ID			0x08
#define PBAP_APP_MISS_CALL_ID		0x09

#define PBAP_APP_ORDER_SIZE			0x01
#define PBAP_APP_SEARCH_ATT_SIZE	0x01
#define PBAP_APP_MAXLIST_SIZE		0x02
#define PBAP_APP_LISTOFFSET_SIZE	0x02
#define PBAP_APP_FILTER_SIZE		0x08
#define PBAP_APP_FORMAT_SIZE		0x01
#define PBAP_APP_PBSIZE_SIZE		0x02
#define PBAP_APP_MISS_CALL_SIZE		0x01

#define PBAP_PCE_UUID ((const uint8_t []) \
{ 0x79, 0x61, 0x35, 0xf0, \
		0xf0, 0xc5, 0x11, 0xd8, 0x09, 0x66, \
		0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66 })

static void debug(const char *msg)
{
	printf("[PCE] %s", msg);
}

static obex_object_t *obex_obj_init(obex_t *obex, const char *name, const char *type)
{
	obex_object_t *obj;
	obex_headerdata_t hd;
	uint8_t uname[200];
	int uname_len;

	obj = OBEX_ObjectNew(obex, OBEX_CMD_GET);
	if (!obj)
		return NULL;

	hd.bq4 = context.connection_id;
	OBEX_ObjectAddHeader(obex, obj, OBEX_HDR_CONNECTION, hd,
			sizeof(hd), OBEX_FL_FIT_ONE_PACKET);

	uname_len = OBEX_CharToUnicode(uname, (uint8_t *) name, sizeof(uname));
	hd.bs = uname;

	OBEX_ObjectAddHeader(obex, obj, OBEX_HDR_NAME,
			hd, uname_len, OBEX_FL_FIT_ONE_PACKET);

	hd.bs = (uint8_t *) type;
	OBEX_ObjectAddHeader(obex, obj, OBEX_HDR_TYPE,
			hd, strlen(type), OBEX_FL_FIT_ONE_PACKET);

	return obj;
}

static int pce_sync(pce_t pce)
{
	while(!pce->finished)
		if (OBEX_HandleInput(pce->obex, 20) <= 0)
			return -1;

	if (!pce->succes)
		return -1;
	return 0;
}

static int pce_sync_request(pce_t pce, obex_object_t obj)
{
	if (!pce->finished)
		return -EBUSY;
	pce->finished = 0;

	OBEX_SetUserData(pce->obex, pce);

	OBEX_Request(pce->obex, obj);

	return pce_sync(pce);
}


static void obex_pce_event(obex_t *obex, obex_object_t *obj, int mode,
		int evt, int cmd, int rsp)
{
	pce_t *pce;

	pce = OBEX_GetUserData(obex);
}

pce_t *PCE_Init(const char *bdaddr, uint8_t channel)
{
	obex_t *obex = NULL;
	pce_t *pce;
	bdaddr_t bd;

	str2ba(bdaddr, bd);
	if (bacmp(bd, BDADDR_ANY) == 0) {
		debug("ERROR Device bt address error!\n");
		return NULL;
	}

	obex = OBEX_Init(OBEX_TRANS_BLUETOOTH, obex_pce_event, 0);
	if (!obex) {
		debug("OBEX_Init failed");
		return NULL;
	}

	if (BtOBEX_TransportConnect(obex, BDADDR_ANY, bdaddr, channel) < 0) {
		debug("Transport connect error!");
		OBEX_Cleanup(obex);
		return NULL;
	}

	/* FIXME glib? */
	pce = g_new0(pce_t, 1);
	pce->obex = obex;
	return pce;
}

int PCE_Connect(pce_t pce)
{
	obex_object_t *obj;
	obex_headerdata_t hd;

	obj = OBEX_ObjectNew(pce->obex, OBEX_CMD_CONNECT);
	if (!obj) {
		debug("Error creating object");
		return -1;
	}

	hd.bs = PBAP_PCE_UUID;
	if (OBEX_ObjectAddHeader(pce->obex, obj, OBEX_HDR_TARGET, hd,
			sizeof(PBAP_PCE_UUID), OBEX_FL_FIT_ONE_PACKET) < 0) {
		debug("Error adding header");
		OBEX_ObjectDelete(pce->obex, obj);
		return -1;
	}

	return pce_sync_request(pce, obj);
}


int PCE_Set_PB(pce_t pce, char *name)
{
	obex_object_t *obj;
	obex_headerdata_t hd;
	uint8_t uname[200];
	uint8_t nohdr_data[2] = { 0x02, 0x00};
	int uname_len = 0;

	obj = OBEX_ObjectNew(pce->obex, OBEX_CMD_SETPATH);
	if (!obj) {
		debug("Error Creating Object (Set Phonebook)\n");
		return -1;
	}

	memset(&hd, 0, sizeof(hd));
	hd.bq4 = context.connection_id;
	OBEX_ObjectAddHeader(pce->obex, obj, OBEX_HDR_CONNECTION, hd,
			sizeof(hd), OBEX_FL_FIT_ONE_PACKET);

	/* FIXME: correct check bad string */
	if (strcmp(name, "..") == 0) {
		/* parent folder */
		nohdr_data[0] |= 1;
	} else if (strcmp(name, "/") != 0) {
		uname_len = OBEX_CharToUnicode(uname, (uint8_t *) name, sizeof(uname));
		hd.bs = uname;
	}

	OBEX_ObjectAddHeader(pce->obex, obj, OBEX_HDR_NAME,
			hd, uname_len, OBEX_FL_FIT_ONE_PACKET);

	OBEX_ObjectSetNonHdrData(obj, nohdr_data, 2);

	return pce_sync_request(pce, obj);
}

int PCE_Pull_PB(pce_t *pce, pce_query_t *query, char **buf)
{
	obex_object_t *obj;
	obex_headerdata_t hd;
	uint8_t app[21];

	obj = obex_obj_init(pce->obex, query->name, XOBEX_BT_PHONEBOOK);
	if (!obj) {
		debug("Error Creating Object (Pull PhoneBook)\n");
		return -1;
	}

	app[0]	= PBAP_APP_FILTER_ID;
	app[1]	= PBAP_APP_FILTER_SIZE;
	app[10]	= PBAP_APP_FORMAT_ID;
	app[11]	= PBAP_APP_FORMAT_SIZE;
	app[13]	= PBAP_APP_MAXLIST_ID;
	app[14]	= PBAP_APP_MAXLIST_SIZE;
	app[17]	= PBAP_APP_LISTOFFSET_ID;
	app[18]	= PBAP_APP_LISTOFFSET_SIZE;

	memcpy(&app[2], query->filter, sizeof(query->filter));
	app[12] = query->format;
	/* FIXME the maxlist default is 0xffff */
	bt_put_unaligned(htons(query->maxlist), (uint16_t *) &app[15]);
	bt_put_unaligned(htons(query->offset), (uint16_t *) &app[19]);

	hd.bs = app;
	OBEX_ObjectAddHeader(pce->obex, obj, OBEX_HDR_APPARAM, hd, sizeof(app), 0);

	return pce_sync_request(pce, obj);
}

int PCE_VCard_List(pce_t *pce, pce_query_t *query, char **buf)
{
	obex_object_t *obj;
	obex_headerdata_t hd;
	uint8_t *app;
	int usearch_len, app_len;

	obj = obex_obj_init(pce->obex, query->name, XOBEX_BT_VCARDLIST);
	if (!obj) {
		debug("Error Creating Object (Pull VCard List)\n");
		return -1;
	}

	usearch_len = OBEX_CharToUnicode(usearch, (uint8_t *) query->search,
			sizeof(usearch));

	app_len = 16 + usearch_len;
	app = g_malloc0(app_len);

	app[0]	= PBAP_APP_ORDER_ID;
	app[1]	= PBAP_APP_ORDER_SIZE;
	app[3]	= PBAP_APP_SEARCH_ATT_ID;
	app[4]	= PBAP_APP_SEARCH_ATT_SIZE;
	app[6]	= PBAP_APP_MAXLIST_ID;
	app[7]	= PBAP_APP_MAXLIST_SIZE;
	app[10]	= PBAP_APP_LISTOFFSET_ID;
	app[11]	= PBAP_APP_LISTOFFSET_SIZE;
	app[14]	= PBAP_APP_SEARCH_VAL_ID;
	app[15]	= usearch_len;

	app[2] = query->order;
	app[5] = query->search_attr;
	bt_put_unaligned(htons(query->maxlist), (uint16_t *) &app[8]);
	bt_put_unaligned(htons(query->offset), (uint16_t *) &app[12]);

	memcpy(&app[16], usearch, usearch_len);

	hd.bs = app;
	OBEX_ObjectAddHeader(pce->obex, obj, OBEX_HDR_APPARAM, hd, app_len, 0);

	g_free(app);

	return pce_sync_request(pce, obj);
}

int PCE_VCard_Entry(pce_t *pce, pce_query_t *query, char **buf)
{
	/* FIXME */
	return 0;
}
