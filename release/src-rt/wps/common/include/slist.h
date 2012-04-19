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
 * $Id: slist.h 241182 2011-02-17 21:50:03Z gmo $
 */

#if !defined SLIST_HDR
#define SLIST_HDR

#ifdef __cplusplus
extern "C" {
#endif

typedef int BOOL;

/*
 * this is an item in the list and stores pointers to data, next and
 * previous list items
 */
typedef struct tag_LISTITEM {
	void *data;
	struct tag_LISTITEM *next;
	struct tag_LISTITEM *prev;
} LISTITEM, * LPLISTITEM;

typedef const LISTITEM * LPCLISTITEM;

/*
 * this is created when Createlist is called and stores the head and tail
 * of the list.
 */
typedef struct tag_LIST {
	LPLISTITEM head;
	LPLISTITEM tail;
	short count;
} TLIST, * LPLIST;

typedef const TLIST * LPCLIST;

/* this is created whenever an iterator is needed for iterating the list */
typedef struct tag_LISTITR {
	LPLIST list;
	LPLISTITEM current;
} LISTITR, * LPLISTITR;

typedef const LISTITR * LPCLISTITR;

/* Function prototypes */

/* List operations */
extern LPLIST list_create(void);
extern BOOL list_addItem(LPLIST list, void * data);
extern BOOL list_removeItem(LPLIST list, void * data);
extern BOOL list_findItem(LPLIST list, void * data);
extern void * list_getFirst(LPLIST list);
extern void * list_getLast(LPLIST list);
extern void* list_getNext(LPLIST list, void * data);

extern int list_getCount(LPLIST list);
extern void list_delete(LPLIST list);
extern void list_freeDelete(LPLIST list);


/* List Iterator operations */
extern LPLISTITR list_itrCreate(LPLIST list);
extern LPLISTITR list_itrFirst(LPLIST list, LPLISTITR listItr);
extern void * list_itrGetNext(LPLISTITR listItr);
extern BOOL list_itrInsertAfter(LPLISTITR listItr, void * data);
extern BOOL list_itrInsertBefore(LPLISTITR listItr, void * data);
extern BOOL list_itrRemoveItem(LPLISTITR listItr);
extern void list_itrDelete(LPLISTITR listItr);

#ifdef __cplusplus
}
#endif


#endif /* SLIST_HDR */
