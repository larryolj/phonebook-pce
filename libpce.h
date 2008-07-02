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

#include <openobex/obex.h>

typedef struct {
	uint32_t 	connection_id;
	obex_t 		*obex;
} pce_t;

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

pce_t *PCE_Connect(const char *bdaddr, uint8_t channel);

int PCE_Set_PB(pce_t *pce, char *name);
int PCE_Pull_PB(pce_t *pce, pce_query_t *query, char **buf);
int PCE_VCard_List(pce_t *pce, pce_query_t  *query, char **buf);
int PCE_VCard_Entry(pce_t *pce, pce_query_t  *query, char **buf);