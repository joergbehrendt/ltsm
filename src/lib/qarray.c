/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License version 2 for more details (a copy is included
 * in the LICENSE file that accompanied this code).
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Copyright (c) 2016, Thomas Stibor <t.stibor@gsi.de>
 */

#include <stdlib.h>
#include "log.h"
#include "qarray.h"


#define DSM_DATE_TO_SEC(date) (date.second + date.minute * 60 +		\
			       date.hour * 3600 + date.day * 86400 +	\
			       date.month * 2678400 + date.year * 977616000)


static query_arr_t *qarray = NULL;
static char *lastKey = NULL;
static long lastSecs = 0;


dsInt16_t init_qarray()
{
	int rc;

	if (qarray)
		return DSM_RC_UNSUCCESSFUL;

	qarray = malloc(sizeof(query_arr_t));
	if (!qarray) {
		rc = errno;
		CT_ERROR(rc, "malloc");
		return rc;
	}

	qarray->data = malloc(sizeof(qryRespArchiveData) * INITIAL_CAPACITY);
	if (!qarray->data) {
		free(qarray);
		rc = errno;
		CT_ERROR(rc, "malloc");
		return rc;
	}
	qarray->capacity = INITIAL_CAPACITY;
	qarray->N = 0;
	lastKey = NULL;
	lastSecs = 0;

	return DSM_RC_SUCCESSFUL;
}

static char * build_key(const qryRespArchiveData *query_data){
	size_t key_len = strlen(query_data->objName.fs) + strlen(query_data->objName.hl) + strlen(query_data->objName.ll) + 1;
	char * key = malloc(key_len);
    snprintf(key, key_len, "%s%s%s", query_data->objName.fs, query_data->objName.hl, query_data->objName.ll);
    return key;
}

dsInt16_t add_query(const qryRespArchiveData *query_data, const dsmBool_t use_latest)
{
	if (!qarray || !qarray->data)
		return DSM_RC_UNSUCCESSFUL;

	if (use_latest) {
		char * datakey = build_key(query_data);
		long secs = DSM_DATE_TO_SEC(query_data->insDate);
		if(lastKey != NULL){
			int cmp = strcmp(lastKey, datakey);
			if(cmp > 0){
				CT_ERROR(cmp, "key of query higher than in last added");
				return DSM_RC_UNSUCCESSFUL;
			}
			if(cmp == 0){
				//lastkey matched current key => overwrite query
				if((secs - lastSecs) < 0){
					CT_ERROR(cmp, "time of query higher than in last added");
					return DSM_RC_UNSUCCESSFUL;
				}
				CT_INFO("Overwrite last query with newer Version: %s  inserted at %i/%i/%i %i:%i:%i",
					datakey,
					query_data->insDate.year,
					query_data->insDate.month,
					query_data->insDate.day,
					query_data->insDate.hour,
					query_data->insDate.minute,
					query_data->insDate.second
						);
				memcpy(&(qarray->data[qarray->N]), query_data, sizeof(qryRespArchiveData));
				return DSM_RC_SUCCESSFUL;
			}
		}
		//lastkey was null or did not match current key => add query
		free(lastKey);
		lastKey = datakey;
		lastSecs = secs;
		CT_INFO("Add new file query: %s  inserted at %i/%i/%i %i:%i:%i",
					datakey,
					query_data->insDate.year,
					query_data->insDate.month,
					query_data->insDate.day,
					query_data->insDate.hour,
					query_data->insDate.minute,
					query_data->insDate.second
				);
	}

	/* Increase length (capacity) by factor of 2 when qarray is full. */
	if (qarray->N >= qarray->capacity) {
		qarray->capacity *= 2;
		qarray->data = realloc(qarray->data,
				       sizeof(qryRespArchiveData) *
				       qarray->capacity);
		if (qarray->data == NULL) {
			CT_ERROR(errno, "realloc");
			return DSM_RC_UNSUCCESSFUL;
		}
	}
	memcpy(&(qarray->data[qarray->N++]), query_data,
	       sizeof(qryRespArchiveData));

	return DSM_RC_SUCCESSFUL;
}

dsInt16_t get_query(qryRespArchiveData *query_data, const unsigned long n)
{
	if (!qarray || !qarray->data)
		return DSM_RC_UNSUCCESSFUL;

	if (n >= qarray->N)
		return DSM_RC_UNSUCCESSFUL;

	*query_data = (qarray->data[n]);

	return DSM_RC_SUCCESSFUL;
}

unsigned long qarray_size()
{
	if (!qarray || !qarray->data)
		return 0;

	return qarray->N;
}

void destroy_qarray()
{
	if (!qarray)
		return;

	if (qarray->data) {
		free(qarray->data);
		qarray->data = NULL;
	}

	free(lastKey);
	free(qarray);
	qarray = NULL;
}

int cmp_restore_order(const void *a, const void *b)
{
	const qryRespArchiveData *query_data_a = (qryRespArchiveData *)a;
	const qryRespArchiveData *query_data_b = (qryRespArchiveData *)b;

	if (query_data_a->restoreOrderExt.top > query_data_b->restoreOrderExt.top)
		return(DS_GREATERTHAN);
	else if (query_data_a->restoreOrderExt.top < query_data_b->restoreOrderExt.top)
		return(DS_LESSTHAN);
	else if (query_data_a->restoreOrderExt.hi_hi > query_data_b->restoreOrderExt.hi_hi)
		return(DS_GREATERTHAN);
	else if (query_data_a->restoreOrderExt.hi_hi < query_data_b->restoreOrderExt.hi_hi)
		return(DS_LESSTHAN);
	else if (query_data_a->restoreOrderExt.hi_lo > query_data_b->restoreOrderExt.hi_lo)
		return(DS_GREATERTHAN);
	else if (query_data_a->restoreOrderExt.hi_lo < query_data_b->restoreOrderExt.hi_lo)
		return(DS_LESSTHAN);
	else if (query_data_a->restoreOrderExt.lo_hi > query_data_b->restoreOrderExt.lo_hi)
		return(DS_GREATERTHAN);
	else if (query_data_a->restoreOrderExt.lo_hi < query_data_b->restoreOrderExt.lo_hi)
		return(DS_LESSTHAN);
	else if (query_data_a->restoreOrderExt.lo_lo > query_data_b->restoreOrderExt.lo_lo)
		return(DS_GREATERTHAN);
	else if (query_data_a->restoreOrderExt.lo_lo < query_data_b->restoreOrderExt.lo_lo)
		return(DS_LESSTHAN);
	else
		return(DS_EQUAL);
}

void sort_qarray()
{
	/* Sort objects to restore on key restoreOrderExt to ensure that tapes are
	   mounted only once and are read from front to back. Sort them in ascending
	   order (low to high). */
	qsort(qarray->data, qarray->N, sizeof(qryRespArchiveData), cmp_restore_order);
}