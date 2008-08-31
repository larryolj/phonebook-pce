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

/* FILTER MASKS */

#define FILTER_VERSION		0X01
#define FILTER_FN		0X01 << 1
#define FILTER_N		0X01 << 2
#define FILTER_PHOTO		0X01 << 3
#define FILTER_BDAY		0X01 << 4
#define FILTER_ADR		0X01 << 5
#define FILTER_LABEL		0X01 << 6
#define FILTER_TEL		0X01 << 7
#define FILTER_EMAIL		0x01 << 8
#define FILTER_MAILER		0x01 << 9
#define FILTER_TZ		0X01 << 10
#define FILTER_GEO		0x01 << 11
#define FILTER_TITLE		0x01 << 12
#define FILTER_ROLE		0x01 << 13
#define FILTER_AGENT		0x01 << 14
#define FILTER_ORG		0x01 << 15
#define FILTER_NOTE		0x01 << 16
#define FILTER_REV		0x01 << 17
#define FILTER_SOUND		0x01 << 18
#define FILTER_URL		0x01 << 19
#define FILTER_UID		0x01 << 20
#define FILTER_KEY		0x01 << 21
#define FILTER_NICKNAME		0x01 << 22
#define FILTER_CATEGORIES	0x01 << 23
#define FILTER_PROID		0x01 << 24
#define FILTER_CLASS		0x01 << 25
#define FILTER_SORT_STRING	0x01 << 26
#define FILTER_X_IRMC_CDT	0x01 << 27

#define FILTER_DEFAULT_21	(FILTER_VERSION | FILTER_FN | FILTER_N | FILTER_TEL)
#define FILTER_DEFAULT_30	(FILTER_VERSION | FILTER_N | FILTER_TEL)

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
	unsigned int	len;
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

static void connect_done(pce_t *self, obex_object_t *obj)
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

	while (OBEX_ObjectGetNextHeader(self->obex, obj, &hi, &hd, &hlen))
		if (hi == OBEX_HDR_CONNECTION)
			self->connection_id = hd.bq4;

	debug("Connect OK!");
}

static void get_done(pce_t *self, obex_object_t *obj, pce_rsp_t *pce_rsp)
{
	obex_headerdata_t hd;
	uint8_t hi, *app = NULL;
	unsigned int hlen;

	debug("Get Done");

	while (OBEX_ObjectGetNextHeader(self->obex, obj, &hi, &hd, &hlen)) {
		switch(hi) {
		case OBEX_HDR_BODY:
			pce_rsp->rsp_id = 0x02;
			if (hlen == 0) {
				pce_rsp->rsp = strdup("");
				break;
			}

			pce_rsp->len = hlen;
			pce_rsp->rsp = malloc(hlen);
			memset(pce_rsp->rsp, 0, pce_rsp->len);
			if (pce_rsp->rsp == NULL) {
				debug("Out of memory");
				break;
			}
			memcpy(pce_rsp->rsp, hd.bs, hlen);
			break;
		case OBEX_HDR_APPARAM:
			app = malloc(hlen);
			memcpy(app, hd.bs, hlen);
			break;
		}
	}

	if (app && app[0] == 0x08) {
		uint16_t size;

		pce_rsp->rsp_id = 0x01;
		pce_rsp->len = sizeof(uint16_t);
		pce_rsp->rsp = malloc(pce_rsp->len);
		size = ntohs(bt_get_unaligned((uint16_t *) &app[2]));
		memcpy(pce_rsp->rsp, &size, pce_rsp->len);
		debug("PhoneBook Size %d", size);
		free(app);
	}
}

static obex_object_t *obex_obj_init(pce_t *self, const char *name,
				const char *type)
{
	obex_object_t *obj;
	obex_headerdata_t hd;
	uint8_t uname[200];
	int uname_len;

	obj = OBEX_ObjectNew(self->obex, OBEX_CMD_GET);
	if (!obj)
		return NULL;

	hd.bq4 = self->connection_id;
	OBEX_ObjectAddHeader(self->obex, obj, OBEX_HDR_CONNECTION, hd,
			sizeof(hd), OBEX_FL_FIT_ONE_PACKET);

	uname_len = OBEX_CharToUnicode(uname, (uint8_t *) name,
					sizeof(uname));
	hd.bs = uname;

	OBEX_ObjectAddHeader(self->obex, obj, OBEX_HDR_NAME,
			hd, uname_len, OBEX_FL_FIT_ONE_PACKET);

	hd.bs = (uint8_t *) type;
	OBEX_ObjectAddHeader(self->obex, obj, OBEX_HDR_TYPE,
			hd, strlen(type), OBEX_FL_FIT_ONE_PACKET);

	return obj;
}

static void pce_req_done(pce_t *self, obex_object_t *obj, int cmd,
				pce_rsp_t *pce_rsp)
{
	switch(cmd) {
	case OBEX_CMD_CONNECT:
		connect_done(self, obj);
		break;
	case OBEX_CMD_DISCONNECT:
		debug("Disconnected!");
		OBEX_TransportDisconnect(self->obex);
		self->connection_id = 0;
		break;
	case OBEX_CMD_GET:
		get_done(self, obj, pce_rsp);
		break;
	case OBEX_CMD_SETPATH:
		break;
	}
}

static void rsp_cleanup(pce_rsp_t *pce_rsp)
{
	if ( pce_rsp == NULL)
		return;
	if (pce_rsp->rsp != NULL)
		free(pce_rsp->rsp);
	free(pce_rsp);
}

static void obex_pce_event(obex_t *obex, obex_object_t *obj, int mode,
			int evt, int cmd, int rsp)
{
	pce_t *self;
	pce_rsp_t *pce_rsp;

	self = OBEX_GetUserData(obex);
	debug("event(0x%02x)/command(0x%02x)", evt, cmd);

	switch(evt) {
	case OBEX_EV_PROGRESS:
	case OBEX_EV_ABORT:
	case OBEX_EV_REQHINT:
	case OBEX_EV_REQ:
	case OBEX_EV_LINKERR:
		break;
	case OBEX_EV_REQDONE:
		pce_rsp = malloc(sizeof(pce_rsp_t));
		if (pce_rsp != NULL) {
			memset(pce_rsp, 0, sizeof(pce_rsp_t));
			pce_rsp->obex_rsp = rsp;
		}
		if (rsp == OBEX_RSP_SUCCESS)
			pce_req_done(self, obj, cmd, pce_rsp);
		self->event_cb(self, pce_rsp, self->userdata);
		rsp_cleanup(pce_rsp);
		break;
	default:
		break;
	}
}

static int request(pce_t *self, obex_object_t *obj, void *userdata)
{
	self->userdata = userdata;

	OBEX_SetUserData(self->obex, self);

	OBEX_Request(self->obex, obj);

	return 0;
}

int PCE_HandleInput(pce_t *self, int timeout)
{
	if (self)
		return OBEX_HandleInput(self->obex, timeout);
	return -1;
}

pce_query_t *PCE_Query_New(const char *name)
{
	pce_query_t *query;

	query = malloc(sizeof(pce_query_t));
	if (query == NULL)
		return NULL;
	memset(query, 0, sizeof(pce_query_t));
	if (name)
		query->name = strdup(name);
	else
		query->name = strdup("");
	query->maxlist = 0xFFFF;
	query->filter = FILTER_DEFAULT_21;

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

pce_t *PCE_Init(const char *bdaddr, uint8_t channel, pce_cb_t event_cb)
{
	obex_t *obex = NULL;
	pce_t *self;
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

	self = malloc(sizeof(pce_t));
	if (self == NULL)
		return NULL;

	memset(self, 0, sizeof(pce_t));
	self->obex = obex;
	self->event_cb = event_cb;

	return self;
}

int PCE_Get_FD(pce_t *self)
{
	if (self)
		return OBEX_GetFD(self->obex);
	return -1;
}

void PCE_Cleanup(pce_t *self)
{
	if (self) {
		OBEX_Cleanup(self->obex);
		free(self);
	}
}

int PCE_Disconnect(pce_t *self, void * data)
{
	debug("Disconnected!");
	OBEX_TransportDisconnect(self->obex);
	self->connection_id = 0;
	//TODO: call done
	return 0;
}

int PCE_Connect(pce_t *self, void * data)
{
	obex_object_t *obj;
	obex_headerdata_t hd;

	obj = OBEX_ObjectNew(self->obex, OBEX_CMD_CONNECT);
	if (!obj) {
		debug("Error creating object");
		return -1;
	}

	hd.bs = PBAP_PCE_UUID;
	if (OBEX_ObjectAddHeader(self->obex, obj, OBEX_HDR_TARGET, hd,
			sizeof(PBAP_PCE_UUID), OBEX_FL_FIT_ONE_PACKET) < 0) {
		debug("Error adding header");
		OBEX_ObjectDelete(self->obex, obj);
		return -1;
	}

	return request(self, obj, data);
}

int PCE_Set_PB(pce_t *self, char *name, void * data)
{
	obex_object_t *obj;
	obex_headerdata_t hd;
	uint8_t uname[200];
	uint8_t nohdr_data[2] = { 0x02, 0x00};
	int uname_len = 0;

	obj = OBEX_ObjectNew(self->obex, OBEX_CMD_SETPATH);
	if (!obj) {
		debug("Error Creating Object (Set Phonebook)");
		return -1;
	}

	memset(&hd, 0, sizeof(hd));
	hd.bq4 = self->connection_id;
	OBEX_ObjectAddHeader(self->obex, obj, OBEX_HDR_CONNECTION, hd,
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

	OBEX_ObjectAddHeader(self->obex, obj, OBEX_HDR_NAME,
			hd, uname_len, OBEX_FL_FIT_ONE_PACKET);

	OBEX_ObjectSetNonHdrData(obj, nohdr_data, 2);

	return request(self, obj, data);
}

int PCE_Pull_PB(pce_t *self, pce_query_t *query, void * data)
{
	obex_object_t *obj;
	obex_headerdata_t hd;
	uint8_t app[21];

	if (!self || !self->obex || !self->connection_id) {
		debug("Not connected");
		return -1;
	}

	obj = obex_obj_init(self, query->name, XOBEX_BT_PHONEBOOK);
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
	OBEX_ObjectAddHeader(self->obex, obj, OBEX_HDR_APPARAM, hd,
						sizeof(app), 0);
	return request(self, obj, data);
}

int PCE_VCard_List(pce_t *self, pce_query_t *query, void * data)
{
	obex_object_t *obj;
	obex_headerdata_t hd;
	uint8_t *app;
	int search_len, app_len;

	if (!self || !self->obex || !self->connection_id) {
		debug("Not connected");
		return -1;
	}

	obj = obex_obj_init(self, query->name, XOBEX_BT_VCARDLIST);
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
		app[14] = PBAP_APP_SEARCH_VAL_ID;
		app[15] = search_len;
		memcpy(&app[16], query->search, search_len);
	}

	hd.bs = app;
	OBEX_ObjectAddHeader(self->obex, obj, OBEX_HDR_APPARAM, hd,
					app_len, 0);

	free(app);

	return request(self, obj, data);
}

int PCE_VCard_Entry(pce_t *self, pce_query_t *query, void * data)
{
	obex_object_t *obj;
	obex_headerdata_t hd;
	uint8_t app[13];

	if (!self || !self->obex || !self->connection_id) {
		debug("Not connected");
		return -1;
	}

	obj = obex_obj_init(self, query->name, XOBEX_BT_VCARD);
	if (!obj) {
		debug("Error Creating Object (Pull VCard Entry)");
		return -1;
	}

	app[0]	= PBAP_APP_FILTER_ID;
	app[1]	= PBAP_APP_FILTER_SIZE;
	app[10]	= PBAP_APP_FORMAT_ID;
	app[11]	= PBAP_APP_FORMAT_SIZE;

	memcpy(&app[2], &query->filter, PBAP_APP_FILTER_SIZE);
	app[12] = query->format;

	hd.bs = app;
	OBEX_ObjectAddHeader(self->obex, obj, OBEX_HDR_APPARAM,
						hd, sizeof(app), 0);

	return request(self, obj, data);
}
