/*
 *
 *  Bluetooth PhoneBook Client Access
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
#include <signal.h>

#include "pce.h"

#define PBAP_PCE_CHANNEL	0x0B

static GMainLoop *main_loop;

static void client_input(pce_t *pce);

static char *folder;

typedef struct {
	int	func_id;
} session_t;

static void start_element_handler(GMarkupParseContext *context,
		const gchar *element, const gchar **names,
		const gchar **values, gpointer user_data, GError **error)
{
	int i;

	if (strcmp(element, "card") != 0)
		return;

	i = 0;
	while (names[i] != NULL) {
		printf("%s: %s ", names[i], values[i]);
		i++;
	}

	printf("\n");
}

static void error_handler (GMarkupParseContext *context, GError *error,
							gpointer user_data)
{
	fprintf (stderr, " %s\n", error->message);
}

static const GMarkupParser parser = {
	start_element_handler,
	NULL,
	NULL,
	NULL,
	error_handler
};

static void sig_term(int sig)
{
	free(folder);
	g_main_loop_quit(main_loop);
}

static void client_help(void)
{
	printf("'c' - Connect Phonebook\n"\
		"'d' - Disconnect Phonebook\n"\
		"'p' - Pull Phonebook\n"\
		"'l' - Pull VCard List\n"\
		"'e' - Pull VCard Entry\n"\
		"'s' - Set Phonebook\n"\
		"'n' - Phonebook Size\n"\
		"'h' - this help\n"\
		"'q' - quit program\n");
}

static char *input_pb(const char *format)
{
	char cmd[10];
	char *name;
	printf("choice Phonebook:\n"\
		"'p' - Phonebook\n"\
		"'i' - Incommin calls\n"\
		"'o' - Outcommin calls\n"\
		"'m' - Missed calls\n"\
		"'c' - Combined calls\n"\
		">> ");
	scanf("%s", cmd);

	switch (cmd[0] | 0x20) {
	case 'i':
		name = g_strdup_printf(format, "ich");
		break;
	case 'o':
		name = g_strdup_printf(format, "och");
		break;
	case 'm':
		name = g_strdup_printf(format,"mch");
		break;
	case 'c':
		name = g_strdup_printf(format,"cch");
		break;
	default:
		name = g_strdup_printf(format,"pb");
		break;
	}

	return name;
}

static void xml_list_parse(const char *xml, ssize_t len)
{
	GMarkupParseContext *p_context;

	p_context = g_markup_parse_context_new(&parser,
		G_MARKUP_DO_NOT_USE_THIS_UNSUPPORTED_FLAG, NULL, NULL);
	if (p_context == NULL) {
		printf("Error in create context\n");
		return;
	}
	g_markup_parse_context_parse(p_context, xml, len, NULL);

	g_free(p_context);
}

static void event_done(pce_t *pce, pce_rsp_t *rsp, void * data)
{
	uint16_t size;
	session_t *s;
	char *vcard;

	s = (session_t *) data;

	printf("obexpb done: response (0x%02x)\n", rsp->obex_rsp);
	switch (rsp->rsp_id) {
	case PBAP_RSP_NONE:
		printf("None data received\n");
		break;
	case PBAP_RSP_SIZE:
		memcpy(&size, rsp->rsp, sizeof(uint16_t));
		printf("Size of phonebook %d\n", size);
		break;
	case PBAP_RSP_BUFF:
		if (s && s->func_id == 2) {
			xml_list_parse((const char *) rsp->rsp, rsp->len);
			break;
		}
		vcard = g_malloc0(rsp->len + 1);
		if (vcard != NULL) {
			strncpy(vcard, (const char *) rsp->rsp, rsp->len);
			printf("%s\n", (const char *) rsp->rsp);
			g_free(vcard);
		}
		break;
	}

	if (data)
		free(data);

	client_input(pce);
}

static int set_phonebook(pce_t *pce)
{
	char name[100];

	printf("Insert folder name, '..' for parent or '/' for root\n>> ");
	scanf("%s", name);

	return PCE_Set_PB(pce, name, NULL);
}

static int pull_vcard_list(pce_t *pce)
{
	pce_query_t *query;
	char *name;
	char search[180];
	session_t *s;

	s = malloc(sizeof(session_t));
	s->func_id = 2;

	name = input_pb("%s");

	printf("Insert search value in %s\n>> ", name);
	scanf("%s", search);

	query = PCE_Query_New((const char *) name);
	free(name);
	query->search = strdup(search);

	if (PCE_VCard_List(pce, query, s) < 0) {
		printf("Pull vcard error\n");
		return -1;
	}

	PCE_Query_Free(query);

	return 0;
}

static int pull_phonebook(pce_t *pce, uint16_t maxlist)
{
	pce_query_t *query;
	char *name;

	name = input_pb("telecom/%s");

	query = PCE_Query_New(name);
	free(name);
	query->maxlist = maxlist;

	if (PCE_Pull_PB(pce, query, NULL) < 0) {
		printf("Pull pb error\n");
		return -1;
	}

	PCE_Query_Free(query);

	return 0;
}

static int pull_vcard_entry(pce_t *pce)
{
	pce_query_t *query;
	char name[200];
	char *uname;

	printf("Insert contact name\n>> ");
	scanf("%s", name);

	uname = g_convert(name, strlen(name), "UTF8", "ASCII", NULL, NULL, NULL);
	query = PCE_Query_New((const char *) uname);
	free(uname);

	if (PCE_VCard_Entry(pce, query, NULL) < 0) {
		printf("Pull entry error\n");
		return -1;
	}

	PCE_Query_Free(query);

	return 0;
}

static void client_input(pce_t *pce)
{
	char cmd[10];
	int run = 1;

	while (run) {
		printf(">> ");
		scanf("%s", cmd);
		run = 0;
		switch (cmd[0] | 0x20) {
		case 'c':
			if (PCE_Connect(pce, NULL) < 0) {
				printf ("Dont Connect to PSE \n");
				run = 1;
			}
			break;
		case 'p':
			pull_phonebook(pce, 0xffff);
			break;
		case 'l':
			pull_vcard_list(pce);
			break;
		case 'e':
			pull_vcard_entry(pce);
			break;
		case 'n':
			pull_phonebook(pce, 0x0);
			break;
		case 's':
			set_phonebook(pce);
			break;
		case 'd':
			PCE_Disconnect(pce, NULL);
			run = 1;
			break;
		case 'q':
			g_main_loop_quit(main_loop);
			break;
		default:
			printf("Command not found!\n");
		case 'h':
			run = 1;
			client_help();
			break;
		}
	}
}

static gboolean watch_cb(GIOChannel *chan, GIOCondition cond, void *data)
{
	pce_t *pce = data;

	if (cond & G_IO_NVAL)
		return FALSE;

	if (cond & (G_IO_HUP | G_IO_ERR)) {
		g_io_channel_close(chan);
		return FALSE;
	}

	PCE_HandleInput(pce, 0);

	return TRUE;
}


int main(int argc, char *argv[])
{
	GIOChannel *io;
	pce_t *pce;
	struct sigaction sa;
	int fd;
	uint8_t channel;

	if (argc < 2) {
		printf("Bluetooth Address missing\nobexpb <address> [<channel>]\n");
		exit(EXIT_FAILURE);
	}

	if (argc >= 3)
		channel = atoi(argv[2]);
	else
		channel = PBAP_PCE_CHANNEL;

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_NOCLDSTOP;
	sa.sa_handler = sig_term;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);

	sa.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);

	pce = PCE_Init(argv[1], channel, event_done);
	if (!pce) {
		printf("Dont initialize PCE\n");
		exit(EXIT_FAILURE);
	}

	main_loop = g_main_loop_new(NULL, FALSE);

	fd = PCE_Get_FD(pce);
	io = g_io_channel_unix_new(fd);
	g_io_add_watch_full(io, G_PRIORITY_DEFAULT,
			G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
			watch_cb, pce,
			(GDestroyNotify) PCE_Cleanup);
	g_io_channel_unref(io);

	PCE_Connect(pce, NULL);

	folder = g_strdup("");

	g_main_loop_run(main_loop);

	g_main_loop_unref(main_loop);

	printf("Exit\n");

	return 0;
}
