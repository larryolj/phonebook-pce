
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <glib.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <openobex/obex.h>
#include <arpa/inet.h>

#define XOBEX_BT_PHONEBOOK "x-bt/phonebook"
#define XOBEX_BT_VCARD "x-bt/vcard"
#define XOBEX_BT_VCARDLIST "x-bt/vcard-listing"

#define PBAP_APP_ORDER_ID 0x01
#define PBAP_APP_SEARCH_VAL_ID 0x02
#define PBAP_APP_SEARCH_ATT_ID 0x03
#define PBAP_APP_MAXLIST_ID 0x04
#define PBAP_APP_LISTOFFSET_ID 0x05
#define PBAP_APP_FILTER_ID 0x06
#define PBAP_APP_FORMAT_ID 0x07
#define PBAP_APP_PBSIZE_ID 0x08
#define PBAP_APP_MISS_CALL_ID 0x09

#define PBAP_APP_ORDER_SIZE 0x01
#define PBAP_APP_SEARCH_ATT_SIZE 0x01
#define PBAP_APP_MAXLIST_SIZE 0x02
#define PBAP_APP_LISTOFFSET_SIZE 0x02
#define PBAP_APP_FILTER_SIZE 0x08
#define PBAP_APP_FORMAT_SIZE 0x01
#define PBAP_APP_PBSIZE_SIZE 0x02
#define PBAP_APP_MISS_CALL_SIZE 0x01

#define PBAP_VCARD_FORMAT_21 0x00
#define PBAP_VCARD_FORMAT_30 0x01

#define PBAP_PCE_CHANNEL 0x10
#define PBAP_PCE_UUID ((const uint8_t []) \
{ 0x79, 0x61, 0x35, 0xf0, \
  0xf0, 0xc5, 0x11, 0xd8, 0x09, 0x66, \
  0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66 })

#define PCE_PULL_FUNC_ID 0x01
#define PCE_LIST_FUNC_ID 0x02
#define PCE_FIND_FUNC_ID 0x03

struct client_context
{
	uint8_t 	func_id;
	bdaddr_t 	bdaddr;
	uint32_t 	connection_id;
	uint8_t 	format;
	uint8_t 	channel;
	obex_t 		*obex;
} context;

static GMainLoop *main_loop;

static void client_disconnect(obex_t *obex);
static obex_t *client_connect(bdaddr_t *bdaddr, uint8_t channel);

static void sig_term(int sig)
{
	g_main_loop_quit(main_loop);
}

static obex_object_t *obex_obj_init(obex_t *obex, const char *name, const char *type)
{
	obex_object_t *obj;
	obex_headerdata_t hd;
	uint8_t u_name[200];
	int u_name_len;

	if(! (obj = OBEX_ObjectNew(obex, OBEX_CMD_GET)))
        return NULL;

	hd.bq4 = context.connection_id;
	OBEX_ObjectAddHeader(obex, obj, OBEX_HDR_CONNECTION, hd,
			sizeof(hd), OBEX_FL_FIT_ONE_PACKET);

	u_name_len = OBEX_CharToUnicode(u_name, (uint8_t *) name, sizeof(u_name));
	hd.bs = u_name;

	OBEX_ObjectAddHeader(obex, obj, OBEX_HDR_NAME,
               hd, u_name_len, OBEX_FL_FIT_ONE_PACKET);

	hd.bs = (uint8_t *) type;
	OBEX_ObjectAddHeader(obex, obj, OBEX_HDR_TYPE,
               hd, strlen(type), OBEX_FL_FIT_ONE_PACKET);

	return obj;
}

static int pull_vcard_list(obex_t *obex, uint8_t order, uint8_t sa,
	   uint16_t maxlist, uint16_t offset)
{
	obex_object_t *obj;
	obex_headerdata_t hd;
	char name[200];
	char search[180];
	uint8_t u_search[180];
	uint8_t *app;
	int u_search_len, app_len;

	if (context.format != PBAP_VCARD_FORMAT_21 &&
			context.format != PBAP_VCARD_FORMAT_30) {
		printf("VCard Format not fould\n");
		return -1;
	}

	printf("Insert folder name\n>> ");
	scanf("%s", name);
	obj = obex_obj_init(obex, name, XOBEX_BT_VCARDLIST);
	if(!obj) {
		printf("Error Creating Object (Pull VCard List)\n");
		return -1;
	}

	printf("Insert search value\n>> ");
	scanf("%s", search);
	u_search_len = OBEX_CharToUnicode(u_search, (uint8_t *) search,
			sizeof(u_search));

	app_len = 16 + u_search_len;
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
	app[15]	= u_search_len;

	app[2] = order;
	app[5] = sa;
	bt_put_unaligned(htons(maxlist), (uint16_t *) &app[8]);
	bt_put_unaligned(htons(offset), (uint16_t *) &app[12]);

	memcpy(&app[16], u_search, u_search_len);

	hd.bs = app;
	OBEX_ObjectAddHeader(obex, obj, OBEX_HDR_APPARAM, hd, app_len, 0);

	OBEX_Request(obex, obj);

	context.func_id = PCE_LIST_FUNC_ID;

	g_free(app);

	return 0;
}

static int pull_phonebook(obex_t *obex, uint64_t filter,
		uint16_t maxlist, uint16_t offset)
{
    obex_object_t *obj;
    obex_headerdata_t hd;
	char name[200];
	uint8_t app[21];

	if (context.format != PBAP_VCARD_FORMAT_21 &&
			context.format != PBAP_VCARD_FORMAT_30) {
		printf("VCard Format not fould\n");
		return -1;
	}

	printf("Insert Phonebook name\n>> ");
	scanf("%s.vcf", name);
	obj = obex_obj_init(obex, name, XOBEX_BT_PHONEBOOK);
	if(!obj) {
		printf("Error Creating Object (Pull PhoneBook)\n");
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

	app[2] = (uint64_t) filter;
	app[12] = context.format;
	bt_put_unaligned(htons(maxlist), (uint16_t *) &app[15]);
	bt_put_unaligned(htons(offset), (uint16_t *) &app[19]);

	hd.bs = app;
	OBEX_ObjectAddHeader(obex, obj, OBEX_HDR_APPARAM, hd, sizeof(app), 0);

	OBEX_Request(obex, obj);

	context.func_id = PCE_PULL_FUNC_ID;

	return 0;
}

static int pull_vcard_entry(obex_t *obex, uint64_t filter)
{
	obex_object_t *obj;
	obex_headerdata_t hd;
	uint8_t app[13];
	char name[200];

	if (context.format != PBAP_VCARD_FORMAT_21 &&
			context.format != PBAP_VCARD_FORMAT_30) {
		printf("VCard Format not fould\n");
		return -1;
	}

	printf("Insert contact name\n>> ");
	scanf("%s.vcf", name);
	obj = obex_obj_init(obex, name, XOBEX_BT_VCARD);
	if(!obj) {
		printf("Error Creating Object (Pull VCard Entry)\n");
		return -1;
	}

	app[0]	= PBAP_APP_FILTER_ID;
	app[1]	= PBAP_APP_FILTER_SIZE;
	app[10]	= PBAP_APP_FORMAT_ID;
	app[11]	= PBAP_APP_FORMAT_SIZE;

	app[2]	= (uint64_t) filter;
	app[12]	= context.format;

	hd.bs = app;
	OBEX_ObjectAddHeader(obex, obj, OBEX_HDR_APPARAM, hd, sizeof(app), 0);

	OBEX_Request(obex, obj);

	context.func_id = PCE_FIND_FUNC_ID;

	return 0;
}

static void get_user_cmd(obex_t *obex)
{
	char cmd[10];
	printf("Select phonebook command\n"\
			"'p' - Pull Phonebook\n"\
			"'l' - Pull VCard List\n"\
			"'e' - Pull VCard Entry\n"\
			"'s' - Set Phonebook\n"\
			"'c' - Connect\n"\
			"'d' - Disconnect\n"\
			"'q' - Quit\n"\
			"------------------------------\n");
init:
	printf(">> ");
	scanf("%s", cmd);
	switch (cmd[0] | 0x20) {
	case 'p':
		if (context.connection_id){
			pull_phonebook(obex, 0xffffffff, 0xffff, 0xff00);
			break;
		}
	case 'l':
		if (context.connection_id){
			pull_vcard_list(obex, 0, 0, 0xffff, 0);
			break;
		}
	case 'e':
		if (context.connection_id){
			pull_vcard_entry(obex, 0);
			break;
		}
	case 'd':
		if (context.connection_id){
			client_disconnect(obex);
			break;
		}
	case 's':
		if (context.connection_id){
			printf("Not Implemented\n");
			goto init;
		}
		printf("Not Connected\n");
		goto init;
	case 'c':
		if (!context.connection_id){
			context.obex = client_connect(&context.bdaddr, context.channel);
			break;
		}
		printf("Alread connected\n");
		goto init;
		break;
	case 'q':
		g_main_loop_quit(main_loop);
		return;
	default:
		printf("Command not found!\n");
		goto init;
	}
}

static void pull_vcard_list_done(obex_t *obex, obex_object_t *obj)
{
	obex_headerdata_t hd;
	uint8_t hi;
	unsigned int hlen;
	char *buf;

	while (OBEX_ObjectGetNextHeader(obex, obj, &hi, &hd, &hlen)){
		if (hi == OBEX_HDR_BODY) {
			buf = g_malloc0(hlen +1);
			strncpy(buf, (char *) hd.bs, hlen);

			printf("PULL LIST DATA\n %s\n", buf);
			g_free(buf);
		}
	}
}

static void pull_vcard_entry_done(obex_t *obex, obex_object_t *obj)
{
	obex_headerdata_t hd;
	uint8_t hi;
	unsigned int hlen;
	char *buf;

	while (OBEX_ObjectGetNextHeader(obex, obj, &hi, &hd, &hlen)){
		if (hi == OBEX_HDR_BODY) {
			buf = g_malloc0(hlen +1);
			strncpy(buf, (char *) hd.bs, hlen);

			printf("PULL ENTRY DATA\n %s\n", buf);
			g_free(buf);
		}
	}
}

static void pull_phonebook_done(obex_t *obex, obex_object_t *obj)
{
	obex_headerdata_t hd;
	uint8_t hi;
	unsigned int hlen;
	char *buf;

	while (OBEX_ObjectGetNextHeader(obex, obj, &hi, &hd, &hlen)){
		if (hi == OBEX_HDR_BODY) {
			buf = g_malloc0(hlen +1);
			strncpy(buf, (char *) hd.bs, hlen);

			printf("PULL PB DATA\n %s\n", buf);
			g_free(buf);
		}
	}
}

static void connect_done(obex_t *obex, obex_object_t *obj, int rsp)
{
	obex_headerdata_t hd;
	uint8_t hi;
	unsigned int hlen;

	if(rsp == OBEX_RSP_SUCCESS) {
		printf("Connect OK!\n");

		while (OBEX_ObjectGetNextHeader(obex, obj, &hi, &hd, &hlen)){
			if (hi == OBEX_HDR_CONNECTION){
				context.connection_id = hd.bq4;
				break;
			}
		}
	}
	else {
		printf("Connect failed 0x%02x!\n", rsp);
	}
}

static void get_done(obex_t *obex, obex_object_t *obj, int rsp)
{
	if(rsp != OBEX_RSP_SUCCESS) {
		printf("Get failed 0x%02x!\n", rsp);
		return;
	}

	switch (context.func_id) {
	case PCE_PULL_FUNC_ID:
		pull_phonebook_done(obex, obj);
		break;
	case PCE_LIST_FUNC_ID:
		pull_vcard_list_done(obex, obj);
		break;
	case PCE_FIND_FUNC_ID:
		pull_vcard_entry_done(obex, obj);
		break;
	default:
		printf("Warning Function Id not fould\n");
	}
}

static void setpath_done(obex_t *obex, obex_object_t *obj, int rsp)
{
	if(rsp == OBEX_RSP_SUCCESS)
		printf("Set path OK!\n");
	else
		printf("Set path failed 0x%02x!\n", rsp);
}

static void pce_req_done(obex_t *obex, obex_object_t *obj, int cmd, int rsp)
{
	switch(cmd) {
	case OBEX_CMD_CONNECT:
		connect_done(obex, obj, rsp);
		break;
	case OBEX_CMD_DISCONNECT:
		printf("Disconnected!\n");
		OBEX_TransportDisconnect(obex);
		context.connection_id = 0;
		break;
	case OBEX_CMD_GET:
		get_done(obex, obj, rsp);
		break;
	case OBEX_CMD_SETPATH:
		setpath_done(obex, obj, rsp);
		break;
	}
	get_user_cmd(obex);
}

static void obex_pce_event(obex_t *obex, obex_object_t *obj, int mode,
	   	int evt, int cmd, int rsp)
{
//	print_event("PCE", evt, cmd, rsp);
	switch (evt) {
	case OBEX_EV_PROGRESS:
		break;
	case OBEX_EV_ABORT:
		break;
	case OBEX_EV_REQDONE:
		pce_req_done(obex, obj, cmd, rsp);
		break;
	case OBEX_EV_REQHINT:
		break;
	case OBEX_EV_REQ:
		break;
	case OBEX_EV_LINKERR:
		break;
	default:
		printf("Unknown event(0x%02x)/command(0x%02x)", evt, cmd);
		break;
	}
}

static void client_disconnect(obex_t *obex)
{
	obex_object_t *obj;

	obj= OBEX_ObjectNew(obex, OBEX_CMD_DISCONNECT);
		if (!obj){
		printf("Error creating object\n");
		return;
	}

	OBEX_Request(obex, obj);
}

gboolean obex_watch_cb(GIOChannel *chan, GIOCondition cond, void *data)
{
	obex_t *obex = data;

	if (cond & G_IO_NVAL)
		return FALSE;

	if (cond & (G_IO_HUP | G_IO_ERR)) {
		g_io_channel_close(chan);
		return FALSE;
	}

	OBEX_HandleInput(obex, 0);

	return TRUE;
}

static obex_t *client_connect(bdaddr_t *bdaddr, uint8_t channel)
{
	obex_t *obex = NULL;
	obex_object_t *obj;
	obex_headerdata_t hd;
	GIOChannel *io;
	int fd;

	if (bacmp(bdaddr, BDADDR_ANY) == 0){
		printf("ERROR: Device bt address error!\n");
		goto fail;
	}

	if(! (obex = OBEX_Init(OBEX_TRANS_BLUETOOTH, obex_pce_event, 0))){
		printf("OBEX_Init failed");
		goto fail;
	}

	if(BtOBEX_TransportConnect(obex, BDADDR_ANY, bdaddr, channel) < 0) {
		printf("Transport connect error!");
		goto fail;
	}

	obj = OBEX_ObjectNew(obex, OBEX_CMD_CONNECT);
	if (!obj){
		printf("Error creating object");
		goto fail;
	}

	hd.bs = PBAP_PCE_UUID;
	if(OBEX_ObjectAddHeader(obex, obj, OBEX_HDR_TARGET,	hd,
			sizeof(PBAP_PCE_UUID), OBEX_FL_FIT_ONE_PACKET) < 0) {
		printf("Error adding header");
		OBEX_ObjectDelete(obex, obj);
		goto fail;
	}

	fd = OBEX_GetFD(obex);
	io = g_io_channel_unix_new(fd);
	g_io_add_watch_full(io, G_PRIORITY_DEFAULT,
			G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
			obex_watch_cb, obex,
			(GDestroyNotify) OBEX_Cleanup);
	g_io_channel_unref(io);

	OBEX_Request(obex, obj);

	return obex;

fail:
	if(obex)
		OBEX_Cleanup(obex);
	printf("ERROR: Creating connection\n");
	return NULL;
}

int main(int argc, char *argv[])
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_NOCLDSTOP;
	sa.sa_handler = sig_term;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT,  &sa, NULL);

	sa.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);

	main_loop = g_main_loop_new(NULL, FALSE);

	if (argc < 2){
		printf("bt address not defined\n");
		exit(0);
	}

	str2ba(argv[1], &context.bdaddr);

	if (argc >= 3)
		context.channel = atoi(argv[2]);
	else
		context.channel = PBAP_PCE_CHANNEL;

	context.obex = client_connect(&context.bdaddr, context.channel);

	g_main_loop_run(main_loop);

	client_disconnect(context.obex);

	g_main_loop_unref(main_loop);

	printf("Exit\n");

	return 0;
}
