/*
 *
 *  Lib PhoneBook Client Access
 *
 *  Copyright (C) 2008  Larry Junior <larry.olj@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <glib.h>

#include <bluetooth/bluetooth.h>
#include <arpa/inet.h>
#include <openobex/obex.h>

#define XOBEX_BT_PHONEBOOK	"x-bt/phonebook"
#define XOBEX_BT_VCARD		"x-bt/vcard"
#define XOBEX_BT_VCARDLIST	"x-bt/vcard-listing"

#define PBAP_APP_ORDER_ID		0x01
#define PBAP_APP_SEARCH_VAL_ID		0x02
#define PBAP_APP_SEARCH_ATT_ID		0x03
#define PBAP_APP_MAXLIST_ID		0x04
#define PBAP_APP_LISTOFFSET_ID		0x05
#define PBAP_APP_FILTER_ID		0x06
#define PBAP_APP_FORMAT_ID		0x07
#define PBAP_APP_PBSIZE_ID		0x08
#define PBAP_APP_MISS_CALL_ID		0x09

#define PBAP_APP_ORDER_SIZE		0x01
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

typedef struct pce pce_t;

typedef struct {
	uint8_t		version;
	uint8_t		flags;
	uint16_t	mtu;
} __attribute__ ((packed)) obex_connect_hdr_t;

/* FIXME: pce_query_t and pce_rsp_t Redefined in obex.h */
typedef struct {
	char		*name;
	char		*search;
	uint64_t	filter;
	uint16_t	maxlist;
	uint16_t	offset;
	uint8_t		search_attr;
	uint8_t		order;
	uint8_t		format;
} pce_query_t;

typedef struct {
	int		obex_rsp;
	int		rsp_id;
	int		len;
	void		*rsp;
} pce_rsp_t;

typedef void (*pce_cb_t)(pce_t *self, pce_rsp_t *rsp, void * data);

struct pce {
	pce_cb_t	event_cb;
	obex_t		*obex;
	void		*userdata;
	pce_rsp_t	*rsp;
	uint32_t	connection_id;
};

static void debug(const char *format, ...)
{
	va_list ap;
	char *msg;

	va_start(ap, format);
	vasprintf(&msg, format, ap);
	va_end(ap);

	printf("[PCE] %s\n", msg);
	free(msg);
}

static void connect_done(pce_t *pce, obex_object_t *obj)
{
	obex_connect_hdr_t *nonhdr;
	obex_headerdata_t hd;
	uint8_t *buffer;
	uint8_t hi;
	uint16_t mtu;
	unsigned int hlen;

	if (OBEX_ObjectGetNonHdrData(obj, &buffer) != sizeof(*nonhdr)) {
		OBEX_ObjectSetRsp(obj, OBEX_RSP_FORBIDDEN, OBEX_RSP_FORBIDDEN);
		debug("Invalid OBEX CONNECT packet");
		return;
	}

	nonhdr = (obex_connect_hdr_t *) buffer;
	mtu = g_ntohs(nonhdr->mtu);
	debug("Version: 0x%02x. Flags: 0x%02x  OBEX packet length: %d",
			nonhdr->version, nonhdr->flags, mtu);

	while (OBEX_ObjectGetNextHeader(pce->obex, obj, &hi, &hd, &hlen))
		if (hi == OBEX_HDR_CONNECTION)
			pce->connection_id = hd.bq4;

	debug("Connect OK!");
}

static void get_done(pce_t *pce, obex_object_t *obj)
{
	obex_headerdata_t hd;
	uint32_t app_len;
	uint8_t hi, *app = NULL;
	unsigned int hlen;

	debug("Get Done");

	while (OBEX_ObjectGetNextHeader(pce->obex, obj, &hi, &hd, &hlen)) {
		switch(hi) {
		case OBEX_HDR_BODY:

			if (hlen == 0) {
				pce->buf = g_strdup("");
				break;
			}

			pce->buf = g_strdup((char *) hd.bs);
			//pce->buf = malloc(hlen +1);
			//strncpy((char *) pce->buf, (char *) hd.bs, hlen);
			break;
		case OBEX_HDR_APPARAM:
			app = malloc(hlen);
			memcpy(app, hd.bs, hlen);
			app_len = hlen;
			break;
		}
	}

	if (app && app[0] == 0x08) {
		uint16_t size;

		size = ntohs(bt_get_unaligned((uint16_t *) &app[2]));
		debug("PhoneBook Size %d", size);
	}
}

static obex_object_t *obex_obj_init(pce_t *pce, const char *name,
				const char *type)
{
	obex_object_t *obj;
	obex_headerdata_t hd;
	uint8_t uname[200];
	int uname_len;

	obj = OBEX_ObjectNew(pce->obex, OBEX_CMD_GET);
	if (!obj)
		return NULL;

	hd.bq4 = pce->connection_id;
	OBEX_ObjectAddHeader(pce->obex, obj, OBEX_HDR_CONNECTION, hd,
			sizeof(hd), OBEX_FL_FIT_ONE_PACKET);

	uname_len = OBEX_CharToUnicode(uname, (uint8_t *) name, sizeof(uname));
	hd.bs = uname;

	OBEX_ObjectAddHeader(pce->obex, obj, OBEX_HDR_NAME,
			hd, uname_len, OBEX_FL_FIT_ONE_PACKET);

	hd.bs = (uint8_t *) type;
	OBEX_ObjectAddHeader(pce->obex, obj, OBEX_HDR_TYPE,
			hd, strlen(type), OBEX_FL_FIT_ONE_PACKET);

	return obj;
}

static void pce_req_done(pce_t *pce, obex_object_t *obj, int cmd)
{
	switch(cmd) {
	case OBEX_CMD_CONNECT:
		connect_done(pce, obj);
		break;
	case OBEX_CMD_DISCONNECT:
		debug("Disconnected!");
		OBEX_TransportDisconnect(pce->obex);
		pce->connection_id = 0;
		break;
	case OBEX_CMD_GET:
		get_done(pce, obj);
		break;
	case OBEX_CMD_SETPATH:
		break;
	}
}

static void obex_pce_event(obex_t *obex, obex_object_t *obj, int mode,
			int evt, int cmd, int rsp)
{
	pce_t *pce;
	pce_cb_t func;

	pce = OBEX_GetUserData(obex);
	debug("event(0x%02x)/command(0x%02x)", evt, cmd);

	switch(evt) {
	case OBEX_EV_PROGRESS:
	case OBEX_EV_ABORT:
	case OBEX_EV_REQHINT:
	case OBEX_EV_REQ:
	case OBEX_EV_LINKERR:
		break;
	case OBEX_EV_REQDONE:
		if (rsp != OBEX_RSP_SUCCESS) {
			debug("Command failed!/response(0x%02x)", rsp);
			break;
		}
		pce_req_done(pce, obj, cmd);
		break;
	default:
		break;
	}

	if (pce->func) {
		func = (pce_cb_t) pce->func;
		func(pce, rsp, pce->buf);
	}
}

int PCE_HandleInput(pce_t *pce, int timeout)
{
	if (pce)
		return OBEX_HandleInput(pce->obex, timeout);
	return -1;
}

pce_query_t *PCE_Query_New(const char *name)
{
	pce_query_t *query;

	query = g_new0(pce_query_t, 1);
	if (name)
		query->name = strdup(name);
	else
		query->name = strdup("");
	query->maxlist = 0xFFFF;

	return query;
}

void PCE_Query_Free(pce_query_t *query)
{
	if (query) {
		if (query->name)
			free(query->name);
		if (query->search)
			free(query->search);
		free(query);
	}
}

pce_t *PCE_Init(const char *bdaddr, uint8_t channel)

{
	obex_t *obex = NULL;
	pce_t *pce;
	bdaddr_t bd;

	str2ba(bdaddr, &bd);
	if (bacmp(&bd, BDADDR_ANY) == 0) {
		debug("ERROR Device bt address error!");
		return NULL;
	}

	obex = OBEX_Init(OBEX_TRANS_BLUETOOTH, obex_pce_event, 0);
	if (!obex) {
		debug("OBEX_Init failed");
		return NULL;
	}

	if (BtOBEX_TransportConnect(obex, BDADDR_ANY, &bd, channel) < 0) {
		debug("Transport connect error!");
		OBEX_Cleanup(obex);
		return NULL;
	}

	pce = g_new0(pce_t, 1);
	pce->obex = obex;
	pce->func = NULL;
	return pce;
}

int PCE_Get_FD(pce_t *pce)
{
	if (pce)
		return OBEX_GetFD(pce->obex);
	return -1;
}

void PCE_Cleanup(pce_t *pce)
{
	OBEX_Cleanup(pce->obex);
	free(pce);
}

int PCE_Disconnect(pce_t *pce)
{
	debug("Disconnected!");
	OBEX_TransportDisconnect(pce->obex);
	pce->connection_id = 0;
	return 0;
}

int PCE_Connect(pce_t *pce, pce_cb_t func)
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

	pce->func = func;

	OBEX_SetUserData(pce->obex, pce);

	OBEX_Request(pce->obex, obj);

	return 0;
}

int PCE_Set_PB(pce_t *pce, char *name, pce_cb_t func)
{
	obex_object_t *obj;
	obex_headerdata_t hd;
	uint8_t uname[200];
	uint8_t nohdr_data[2] = { 0x02, 0x00};
	int uname_len = 0;

	obj = OBEX_ObjectNew(pce->obex, OBEX_CMD_SETPATH);
	if (!obj) {
		debug("Error Creating Object (Set Phonebook)");
		return -1;
	}

	memset(&hd, 0, sizeof(hd));
	hd.bq4 = pce->connection_id;
	OBEX_ObjectAddHeader(pce->obex, obj, OBEX_HDR_CONNECTION, hd,
			sizeof(hd), OBEX_FL_FIT_ONE_PACKET);

	/* FIXME: correct check bad string */
	if (strcmp(name, "..") == 0) {
		/* parent folder */
		nohdr_data[0] |= 1;
	} else if (strcmp(name, "/") != 0) {
		uname_len = OBEX_CharToUnicode(uname, (uint8_t *) name,
						sizeof(uname));
		hd.bs = uname;
	}

	OBEX_ObjectAddHeader(pce->obex, obj, OBEX_HDR_NAME,
			hd, uname_len, OBEX_FL_FIT_ONE_PACKET);

	OBEX_ObjectSetNonHdrData(obj, nohdr_data, 2);

	pce->func = func;

	OBEX_SetUserData(pce->obex, pce);

	OBEX_Request(pce->obex, obj);

	return 0;
}

int PCE_Pull_PB(pce_t *pce, pce_query_t *query, pce_cb_t func)
{
	obex_object_t *obj;
	obex_headerdata_t hd;
	uint8_t app[21];

	if (!pce || !pce->obex || !pce->connection_id) {
		debug("Not connected");
		return -1;
	}

	obj = obex_obj_init(pce, query->name, XOBEX_BT_PHONEBOOK);
	if (!obj) {
		debug("Error Creating Object (Pull PhoneBook)");
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

	memcpy(&app[2], &query->filter, PBAP_APP_FILTER_SIZE);
	app[12] = query->format;
	bt_put_unaligned(htons(query->maxlist), (uint16_t *) &app[15]);
	bt_put_unaligned(htons(query->offset), (uint16_t *) &app[19]);

	hd.bs = app;
	OBEX_ObjectAddHeader(pce->obex, obj, OBEX_HDR_APPARAM, hd,
						sizeof(app), 0);

	pce->func = func;

	OBEX_SetUserData(pce->obex, pce);

	OBEX_Request(pce->obex, obj);

	return 0;
}

int PCE_VCard_List(pce_t *pce, pce_query_t *query, pce_cb_t func)
{
	obex_object_t *obj;
	obex_headerdata_t hd;
	uint8_t *app;
	int search_len, app_len;

	if (!pce || !pce->obex || !pce->connection_id) {
		debug("Not connected");
		return -1;
	}

	obj = obex_obj_init(pce, query->name, XOBEX_BT_VCARDLIST);
	if (!obj) {
		debug("Error Creating Object (Pull VCard List)");
		return -1;
	}

	search_len = 0;
	app_len = 14;

	if (query->search) {
		search_len = strlen(query->search);
		app_len += 2 + search_len;
	}

	app = malloc(app_len);

	app[0]	= PBAP_APP_ORDER_ID;
	app[1]	= PBAP_APP_ORDER_SIZE;
	app[3]	= PBAP_APP_SEARCH_ATT_ID;
	app[4]	= PBAP_APP_SEARCH_ATT_SIZE;
	app[6]	= PBAP_APP_MAXLIST_ID;
	app[7]	= PBAP_APP_MAXLIST_SIZE;
	app[10]	= PBAP_APP_LISTOFFSET_ID;
	app[11]	= PBAP_APP_LISTOFFSET_SIZE;

	app[2] = query->order;
	app[5] = query->search_attr;
	bt_put_unaligned(htons(query->maxlist), (uint16_t *) &app[8]);
	bt_put_unaligned(htons(query->offset), (uint16_t *) &app[12]);

	if (search_len > 0) {
		app[14]	= PBAP_APP_SEARCH_VAL_ID;
		app[15]	= search_len;
		memcpy(&app[16], query->search, search_len);
	}

	hd.bs = app;
	OBEX_ObjectAddHeader(pce->obex, obj, OBEX_HDR_APPARAM, hd, app_len, 0);

	free(app);

	pce->func = func;

	OBEX_SetUserData(pce->obex, pce);

	OBEX_Request(pce->obex, obj);

	return 0;
}

int PCE_VCard_Entry(pce_t *pce, pce_query_t *query, pce_cb_t func)
{
	obex_object_t *obj;
	obex_headerdata_t hd;
	uint8_t app[13];

	if (!pce || !pce->obex || !pce->connection_id) {
		debug("Not connected");
		return -1;
	}

	obj = obex_obj_init(pce, query->name, XOBEX_BT_VCARD);
	if (!obj) {
		debug("Error Creating Object (Pull VCard Entry)");
		return -1;
	}

	app[0]	= PBAP_APP_FILTER_ID;
	app[1]	= PBAP_APP_FILTER_SIZE;
	app[10]	= PBAP_APP_FORMAT_ID;
	app[11]	= PBAP_APP_FORMAT_SIZE;

	memcpy(&app[2], &query->filter, PBAP_APP_FILTER_SIZE);
	app[12]	= query->format;

	hd.bs = app;
	OBEX_ObjectAddHeader(pce->obex, obj, OBEX_HDR_APPARAM,
						hd, sizeof(app), 0);

	pce->func = func;

	OBEX_SetUserData(pce->obex, pce);

	OBEX_Request(pce->obex, obj);

	return 0;
}
