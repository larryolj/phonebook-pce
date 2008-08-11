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

#include "pce.h"

#define PBAP_PCE_CHANNEL		0x0A

static void client_help()
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

static int set_phonebook(pce_t *pce)
{
	char name[100];

	printf("Insert folder name, '..' for parent or '/' for root\n>> ");
	scanf("%s", name);

	return PCE_Set_PB(pce, name);
}

static int pull_vcard_list(pce_t *pce)
{
	pce_query_t *query;
	char name[200];
	char search[180];
	char *buf;

	printf("Insert folder name\n>> ");
	scanf("%s", name);

	printf("Insert search value\n>> ");
	scanf("%s", search);

	query = PCE_Query_New((const char *) name);
	query->search = strdup(search);

	if (PCE_VCard_List(pce, query, &buf) < 0) {
		printf("Pull vcard error\n");
		return -1;
	}

	printf("Pull vcard:\n %s\n", buf);
	free(buf);
	PCE_Query_Free(query);

	return 0;
}

static int pull_phonebook(pce_t *pce, uint16_t maxlist)
{
	pce_query_t *query;
	char name[200];
	char *buf;

	printf("Insert Phonebook name\n>> ");
	scanf("%s.vcf", name);

	query = PCE_Query_New((const char *) name);

	if (PCE_Pull_PB(pce, query, &buf) < 0) {
		printf("Pull pb error\n");
		return -1;
	}

	printf("Pull pb:\n %s\n", buf);
	free(buf);
	PCE_Query_Free(query);

	return 0;
}

static int pull_vcard_entry(pce_t *pce)
{
	pce_query_t *query;
	char name[200];
	char *buf;

	printf("Insert contact name\n>> ");
	scanf("%s.vcf", name);

	query = PCE_Query_New((const char *) name);

	if (PCE_VCard_Entry(pce, query, &buf) < 0) {
		printf("Pull entry error\n");
		return -1;
	}

	printf("Pull entry:\n %s\n", buf);
	free(buf);
	PCE_Query_Free(query);

	return 0;
}

int main(int argc, char *argv[])
{
	pce_t *pce;
	uint8_t channel;
	char cmd[10];
	int run = 1;

	if (argc < 2) {
		printf("Bluetooth Address missing\n");
		exit(EXIT_FAILURE);
	}

	if (argc >= 3)
		channel = atoi(argv[2]);
	else
		channel = PBAP_PCE_CHANNEL;

	pce = PCE_Init(argv[1], channel);
	if (!pce) {
		printf("Dont initialize PCE\n");
		exit(EXIT_FAILURE);
	}

	while (run) {
		printf(">> ");
		scanf("%s", cmd);
		switch (cmd[0] | 0x20) {
		case 'c':
			if (PCE_Connect(pce) < 0)
				printf ("Dont Connect to PSE \n");
			break;
		case 'p':
			if (pce->connection_id) {
				pull_phonebook(pce, 0xffff);
				break;
			}
		case 'l':
			if (pce->connection_id) {
				pull_vcard_list(pce);
				break;
			}
		case 'e':
			if (pce->connection_id) {
				pull_vcard_entry(pce);
				break;
			}
		case 'n':
			if (pce->connection_id) {
				pull_phonebook(pce, 0x0);
				break;
			}
		case 's':
			if (pce->connection_id) {
				set_phonebook(pce);
				break;
			}
		case 'd':
			if (pce->connection_id) {
				PCE_Disconnect(pce);
				break;
			}
			printf("Not Connected\n");
			break;
		case 'h':
			client_help();
			break;
		case 'q':
			run = 0;
			break;
		default:
			client_help();
			printf("Command not found!\n");
		}
	}
	PCE_Cleanup(pce);

	printf("Exit\n");

	return 0;
}
