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

#include <openobex/obex.h>

#define PBAP_VCARD_FORMAT_21	0x00
#define PBAP_VCARD_FORMAT_30	0x01

#define PBAP_ORDER_INDEXED 0x00
#define PBAP_ORDER_ALPHANUN 0x01
#define PBAP_ORDER_PHONETIC 0x02

#define PBAP_SEARCHATTR_NAME 0x00
#define PBAP_SEARCHATTR_NUMBER 0x01
#define PBAP_SEARCHATTR_SOUND 0x02
#define PBAP_RSP_NONE 0x00
#define PBAP_RSP_SIZE 0x01
#define PBAP_RSP_BUFF 0x02

typedef void *pce_t;

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

pce_query_t *PCE_Query_New(const char *name);

void PCE_Query_Free(pce_query_t *query);

pce_t *PCE_Init(const char *bdaddr, uint8_t channel, pce_cb_t event_cb);

int PCE_HandleInput(pce_t *self, int timeout);

int PCE_Get_FD(pce_t *self);

void PCE_Cleanup(pce_t *self);

int PCE_Disconnect(pce_t *self, void * data);

int PCE_Connect(pce_t *self, void * data);

int PCE_Set_PB(pce_t *self, char *name, void * data);

int PCE_Pull_PB(pce_t *self, pce_query_t *query, void * data);

int PCE_VCard_List(pce_t *self, pce_query_t  *query,  void * data);

int PCE_VCard_Entry(pce_t *self, pce_query_t  *query, void * data);


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
