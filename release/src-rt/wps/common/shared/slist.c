/*
 * Slist
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: slist.c 274160 2011-07-28 10:52:54Z kenlo $
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <slist.h>

/* #defines used in this file only */
#if defined(WIN32)
#include <windows.h>
#define FIRST_TIME INVALID_HANDLE_VALUE
#else
#define FIRST_TIME 0xFFFFFFFF
#endif

#define FALSE 0
#define TRUE  1

LPLIST
list_create(void)
{
	LPLIST list;

	list = (LPLIST) calloc(1, sizeof(TLIST));

#ifdef _LISTDEBUG
	CSTRACE((CSTRACE_INFO, "list_create %X\n", list));
#endif /* _LISTDEBUG */

	if (list) {
		/* initialize data members */
		list->head = NULL;
		list->tail = NULL;
		list->count = 0;
	}
	return list;
}

BOOL
list_addItem(LPLIST list, void * data)
{
	LPLISTITEM newListItem;

	assert(list);
	assert(data);

	newListItem = (LPLISTITEM) calloc(1, sizeof(LISTITEM));
	if (newListItem) {
		newListItem->data = data;
		newListItem->next = NULL;

		if (list->head == NULL) {
			/* no list items right now */
			list->head = list->tail = newListItem;
			newListItem->prev = NULL;
		}
		else {
			/* there are one or more item */
			newListItem->prev = list->tail;
			list->tail->next = newListItem;
			list->tail = newListItem;
		}
		/* increment number of items */
		list->count ++;
		return TRUE;
	}
	else {
		return FALSE;
	}
}

BOOL
list_removeItem(LPLIST list, void * data)
{
	LPLISTITEM current;

	assert(list);
	assert(data);

	current = list->head;
	while (current) {
		if (current->data == data) {
			if (current->next == NULL && current->prev == NULL) {
				/* this is the only item */
				list->head = list->tail = NULL;
			}
			else if (current->next == NULL) {
				/* this is the last item */
				list->tail = current->prev;
				current->prev->next = NULL;
			}
			else if (current->prev == NULL) {
				/* this is the first item */
				list->head = current->next;
				current->next->prev = NULL;
			}
			else {
				/* somewhere in the middle */
				current->next->prev = current->prev;
				current->prev->next = current->next;
			}
			free(current);
			list->count --;
			return TRUE;
		}
		/* traverse further */
		current = current->next;
	}
	return FALSE;
}

BOOL
list_findItem(LPLIST list, void * data)
{
	LPLISTITEM current;

	assert(list);
	assert(data);

	current = list->head;
	while (current) {
		if (current->data == data) {
			return TRUE;
		}
		/* traverse further */
		current = current->next;
	}
	return FALSE;
}

void *
list_getFirst(LPLIST list)
{
	if (list->head) {
		return list->head->data;
	}
	else {
		return NULL;
	}
}

void *
list_getLast(LPLIST list)
{
	if (list->tail) {
		return list->tail->data;
	}
	else {
		return NULL;
	}
}

void*
list_getNext(LPLIST list, void * data)
{
	LPLISTITEM current;

	assert(list);
	assert(data);

	current = list->head;
	while (current) {
		if (current->data == data) {
			break;
		}
		/* traverse further */
		current = current->next;
	}

	if (current == NULL || current->next == NULL)
		return NULL;

	return current->next->data;
}

int
list_getCount(LPLIST list)
{
	assert(list);

	return (list->count);
}

void
list_delete(LPLIST list)
{
	LPLISTITEM current;
	LPLISTITEM tmpItem;

	if (!list)
		return;

	current = list->head;
	while (current) {
		tmpItem = current->next;
		free(current);
		list->count --;
		current = tmpItem;
	}
#ifdef _LISTDEBUG
	CSTRACE((CSTRACE_INFO, "list_delete %X\n", list));
#endif /* _LISTDEBUG */

	free(list);
}

void
list_freeDelete(LPLIST list)
{
	LPLISTITEM current;
	LPLISTITEM tmpItem;

	assert(list);

	current = list->head;
	while (current) {
		tmpItem = current->next;
		free(current->data);
		free(current);
		list->count --;
		current = tmpItem;
	}

#ifdef _LISTDEBUG
	CSTRACE((CSTRACE_INFO, "list_freeDelete %X\n", list));
#endif /* _LISTDEBUG */

	free(list);
}

LPLISTITR
list_itrCreate(LPLIST list)
{
	LPLISTITR listItr;

	assert(list);

	listItr = (LPLISTITR) calloc(1, sizeof(LISTITR));
#ifdef _LISTDEBUG
	CSTRACE((CSTRACE_INFO, "list_itrCreate %X\n", listItr));
#endif /* _LISTDEBUG */

	if (listItr) {
		/* initialize data members */
		listItr->list = list;
		listItr->current = (LPLISTITEM) FIRST_TIME;
	}

	return listItr;
}

LPLISTITR
list_itrFirst(LPLIST list, LPLISTITR listItr)
{
	assert(list);

	if (listItr) {
		/* initialize data members */
		listItr->list = list;
		listItr->current = (LPLISTITEM) FIRST_TIME;
	}

	return listItr;
}

void *
list_itrGetNext(LPLISTITR listItr)
{
	assert(listItr);

	if (listItr->current == (LPLISTITEM)FIRST_TIME) {
		listItr->current = listItr->list->head;
	}
	else if (listItr->current == NULL) {
		return NULL;
	}
	else {
		listItr->current = listItr->current->next;
	}

	if (listItr->current) {
		return listItr->current->data;
	}
	else {
		return NULL;
	}
}

BOOL
list_itrInsertAfter(LPLISTITR listItr, void * data)
{
	LPLISTITEM newListItem;

	assert(listItr);
	assert(data);
	assert(listItr->current != (LPLISTITEM) FIRST_TIME);

	newListItem = (LPLISTITEM) calloc(1, sizeof(LISTITEM));
	if (newListItem) {
		newListItem->data = data;
		if (listItr->current == NULL) {
			if (listItr->list->count == 0) {
				/* there are no items right now */
				listItr->list->head = listItr->list->tail = newListItem;
				listItr->current = listItr->list->head;
			}
			else {
				/* there are some items, current points to null */
				newListItem->prev = listItr->list->tail;
				newListItem->next = NULL;
				listItr->list->tail->next = newListItem;
				listItr->list->tail = newListItem;
				listItr->current = newListItem;
			}
		}
		else {
			newListItem->prev = listItr->current;
			newListItem->next = listItr->current->next;
			if (listItr->current->next) {
				/* there are items after */
				listItr->current->next->prev = newListItem;
			}
			else {
				/* last item so move the tail */
				listItr->list->tail = newListItem;
			}
			listItr->current->next = newListItem;
		}
		/* increment number of items */
		listItr->list->count ++;
		return TRUE;
	}
	else {
		return FALSE;
	}
}

BOOL
list_itrInsertBefore(LPLISTITR listItr, void * data)
{
	LPLISTITEM newListItem;

	assert(listItr);
	assert(data);
	assert(listItr->current != (LPLISTITEM) FIRST_TIME);

	newListItem = (LPLISTITEM) calloc(1, sizeof(LISTITEM));
	if (newListItem) {
		newListItem->data = data;
		if (listItr->current == NULL) {
			if (listItr->list->count == 0) {
				/* there are no items right now */
				listItr->list->head = listItr->list->tail = newListItem;
				listItr->current = listItr->list->head;
			}
			else {
				/* there are some items, current points to null */
				newListItem->prev = listItr->list->tail;
				newListItem->next = NULL;
				listItr->list->tail->next = newListItem;
				listItr->list->tail = newListItem;
				listItr->current = newListItem;
			}
		}
		else {
			newListItem->next = listItr->current;
			newListItem->prev = listItr->current->prev;
			if (listItr->current->prev) {
				/* there are items before */
				listItr->current->prev->next = newListItem;
			}
			else {
				/* first item so move the head */
				listItr->list->head = newListItem;
			}
			listItr->current->prev = newListItem;
		}
		/* increment number of items */
		listItr->list->count ++;
		return TRUE;
	}
	else {
		return FALSE;
	}
}

BOOL
list_itrRemoveItem(LPLISTITR listItr)
{
	LPLISTITEM tmpItem;

	assert(listItr);
	assert(listItr->current);

	/* not yet iterated */
	if (listItr->current == (LPLISTITEM) FIRST_TIME) {
		return FALSE;
	}

	if (listItr->current->next == NULL && listItr->current->prev == NULL) {
		/* this is the only item */
		listItr->list->head = listItr->list->tail = NULL;
		tmpItem = NULL;
	}
	else if (listItr->current->next == NULL) {
		/* this is the last item */
		listItr->list->tail = listItr->current->prev;
		listItr->current->prev->next = NULL;
		tmpItem = listItr->current->prev;
	}
	else if (listItr->current->prev == NULL) {
		/* this is the first item */
		listItr->list->head = listItr->current->next;
		listItr->current->next->prev = NULL;
		tmpItem = (LPLISTITEM) FIRST_TIME;
	}
	else {
		/* somewhere in the middle */
		listItr->current->next->prev = listItr->current->prev;
		listItr->current->prev->next = listItr->current->next;
		tmpItem = listItr->current->prev;
	}
	free(listItr->current);
	listItr->current = tmpItem;
	listItr->list->count --;
	return TRUE;
}

void
list_itrDelete(LPLISTITR listItr)
{
	assert(listItr);

#ifdef _LISTDEBUG
	CSTRACE((CSTRACE_INFO, "list_itrDelete %X\n", listItr));
#endif /* _LISTDEBUG */

	free(listItr);
}
