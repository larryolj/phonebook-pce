/*
 *
 *  Bluetooth PhoneBook Client Access
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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <glib.h>

#include <arpa/inet.h>
#include "libpce.h"

#define PBAP_PCE_CHANNEL		0x10

static GMainLoop *main_loop;

static void client_disconnect(obex_t *obex);
static obex_t *client_connect(bdaddr_t *bdaddr, uint8_t channel);

static void sig_term(int sig)
{
	g_main_loop_quit(main_loop);
}

static int set_phonebook(pce_t *pce)
{
	char name[100];

	printf("Insert folder name, '..' for parent or '\\' for root\n>> ");
	scanf("%s", name);

	return PCE_Set_PB(pce, name);
}

static int pull_vcard_list(pce_t *pce)
{
	pce_query_t query;
	char name[200];
	char search[180];
	char *buf;

	printf("Insert folder name\n>> ");
	scanf("%s", name);

	printf("Insert search value\n>> ");
	scanf("%s", search);

	query.name = &name;
	query.search = &search;
	query.maxlist = 0xFFFF;
	if (PCE_VCard_List(pce, &query, &buf) < 0) {
		printf("Pull vcard error");
		return -1;
	}
	printf("Pull vcard:\n %s\n", buf);

	return 0;
}

static int pull_phonebook(obex_t *obex, uint16_t maxlist)
{
	pce_query_t query;
	char name[200];
	char *buf;

	printf("Insert Phonebook name\n>> ");
	scanf("%s.vcf", name);

	query.name = &name;
	query.maxlist = maxlist;

	if (PCE_Pull_PB(pce, &query, &buf) < 0) {
		printf("Pull pb error");
		return -1;
	}

	printf("Pull pb:\n %s\n", buf);

	return 0;
}

static int pull_vcard_entry(pce_t *pce)
{
	pce_query_t query;
	char name[200];
	char *buf;

	printf("Insert contact name\n>> ");
	scanf("%s.vcf", name);

	query.name = &name;

	if (PCE_VCard_Entry(pce, &query, &buf) < 0) {
		printf("Pull entry error");
		return -1;
	}

	printf("Pull entry:\n %s\n", buf);
	return 0;
}

static void user_cmd(pce_t *pc, char cmd)
{
	switch (cmd) {
	case 'h':
		printf("Select phonebook command\n"\
			"'p' - Pull Phonebook\n"\
			"'l' - Pull VCard List\n"\
			"'e' - Pull VCard Entry\n"\
			"'s' - Set Phonebook\n"\
			"'n' - Phonebook Size\n");
	case 'p':
		if (pce.connection_id) {
			pull_phonebook(pce, 0xffff);
			break;
		}
	case 'l':
		if (pce.connection_id) {
			pull_vcard_list(pce);
			break;
		}
	case 'e':
		if (pce.connection_id) {
			pull_vcard_entry(pce);
			break;
		}
	case 'n':
		if (pce.connection_id) {
			pull_phonebook(pce, 0x0);
			break;
		}
	case 's':
		if (pce.connection_id) {
			set_phonebook(obex);
			break;
		}
		printf("Not Connected\n");
	default:
		printf("Command not found!\n");
	}
}

int main(int argc, char *argv[])
{
	pce_t *pce;
	char *cmd;

	if (argc < 2) {
		fprintf(stderr, "Bluetooth Address missing\n");
		exit(EXIT_FAILURE);
	}

	pce = PCE_Init(argv[1], PBAP_PCE_CHANNE);

	if (argc > 2){
		cmd = argv[2];
		user_cmd(pce, cmd[1])
	}

	PCE_Disconnect(pce);

	printf("Exit\n");

	return 0;
}
