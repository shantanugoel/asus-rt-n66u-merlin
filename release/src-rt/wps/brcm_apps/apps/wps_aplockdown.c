/*
 * WPS aplockdown
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_aplockdown.c,v 1.8 2010-12-17 02:23:10 Exp $
 */

#include <stdio.h>
#include <time.h>
#include <tutrace.h>
#include <wps.h>
#include <wps_wl.h>
#include <wps_aplockdown.h>
#include <wps_ui.h>
#include <wps_ie.h>

typedef struct wps_aplockdown_node {
	struct wps_aplockdown_node *next;
	unsigned long access_time;
} WPS_APLOCKDOWN_NODE;

typedef struct {
	WPS_APLOCKDOWN_NODE *head;
	WPS_APLOCKDOWN_NODE *tail;
	unsigned int force_on;
	unsigned int enabled;
	unsigned int locked;
	unsigned int living_cnt;
	unsigned int limit_cnt;
	unsigned int duration;
	unsigned int ageout;
  unsigned int init_ageout;	
} WPS_APLOCKDOWN;

/* How many failed PIN authentication attempts */
#define WPS_APLOCKDOWN_LIMIT		3
#define WPS_V2_APLOCKDOWN_LIMIT		3

/* Within how many seconds */
#define WPS_APLOCKDOWN_DURATION		60
#define WPS_V2_APLOCKDOWN_DURATION	60

/* How many seconds AP must stay in the lock-down state */
#define WPS_APLOCKDOWN_AGEOUT		60
#define WPS_V2_APLOCKDOWN_AGEOUT	60

static WPS_APLOCKDOWN wps_aplockdown;

int
wps_aplockdown_init()
{
	char *p = NULL;
	uint32 limit = 0;
	uint32 duration = 0;
	uint32 ageout = 0;

	memset(&wps_aplockdown, 0, sizeof(wps_aplockdown));

	if (!strcmp(wps_safe_get_conf("wps_aplockdown_forceon"), "1"))
		wps_aplockdown.force_on = 1;

	/* wps_aplockdown_cap not equal to 1 */
	if (strcmp(wps_safe_get_conf("wps_aplockdown_cap"), "1") != 0) {
		TUTRACE((TUTRACE_INFO, "WPS AP lock down capability is disabling!\n"));
		return 0;
	}

	wps_aplockdown.enabled = 1;

	/* WSC 2.0 */
	if (strcmp(wps_safe_get_conf("wps_version2"), "enabled") == 0) {
		limit = WPS_V2_APLOCKDOWN_LIMIT;
		duration = WPS_V2_APLOCKDOWN_DURATION;
		ageout = WPS_V2_APLOCKDOWN_AGEOUT;
	}
	else {
		limit = WPS_APLOCKDOWN_LIMIT;
		duration = WPS_APLOCKDOWN_DURATION;
		ageout = WPS_APLOCKDOWN_AGEOUT;
	}

	if ((p = wps_get_conf("wps_aplockdown_count")))
		limit = atoi(p);

	if ((p = wps_get_conf("wps_aplockdown_duration")))
		duration = atoi(p);

	if ((p = wps_get_conf("wps_aplockdown_ageout")))
		ageout = atoi(p);

	wps_aplockdown.limit_cnt = limit;
	wps_aplockdown.duration = duration;
	wps_aplockdown.ageout = 0;
	wps_aplockdown.init_ageout = ageout;

	TUTRACE((TUTRACE_INFO, "WPS aplockdown init: limit = %d, duration = %d, ageout = %d!\n",
		limit, duration, ageout));

	return 0;
}

int
wps_aplockdown_add(void)
{
	unsigned long now;
	WPS_APLOCKDOWN_NODE *curr = NULL;
	WPS_APLOCKDOWN_NODE *prev = NULL;
	WPS_APLOCKDOWN_NODE *next = NULL;

	if (wps_aplockdown.enabled == 0)
		return 0;

	if (wps_aplockdown.locked == 1)
		return 0;

	time(&now);

	TUTRACE((TUTRACE_INFO, "Error of config AP pin fail in %d!\n", now));

	curr = (WPS_APLOCKDOWN_NODE *)malloc(sizeof(*curr));
	if (!curr) {
		TUTRACE((TUTRACE_INFO, "Memory allocation for WPS aplockdown service fail!!\n"));
		return -1;
	}

	curr->access_time = now;
	curr->next = wps_aplockdown.head;

	/*
	 * Add an WPS_APLOCKDOWN_NODE to list
	 */
	wps_aplockdown.head = curr;
	if (wps_aplockdown.tail == NULL)
		wps_aplockdown.tail = curr;

	wps_aplockdown.living_cnt++;

	/*
	 * If hit AP setup lock criteria,
	 * check duration
	 */
	if (wps_aplockdown.living_cnt >= wps_aplockdown.limit_cnt) {
		if ((curr->access_time - wps_aplockdown.tail->access_time)
		    <= wps_aplockdown.duration) {
			/* set apLockDown mode indicator as true */

			wps_aplockdown.locked = 1;
			if (wps_aplockdown.ageout == 0)
			 wps_aplockdown.ageout = wps_aplockdown.init_ageout;
			else { 
			 /* Maximum lockout period is one year */
			  if (wps_aplockdown.ageout < 60*60*24*365)
			    wps_aplockdown.ageout *= 2;
      }

			wps_ui_set_env("wps_aplockdown", "1");
			wps_set_conf("wps_aplockdown", "1");

			/* reset the IE */
			wps_ie_set(NULL, NULL);

			TUTRACE((TUTRACE_ERR, "AP is lock down now\n"));
			return 1;
		}
	}

	/*
	 * Age out nodes in list
	 */
	wps_aplockdown.living_cnt = 0;
	curr = prev = wps_aplockdown.head;
	while (curr) {
		/*
		 * Hit the age out criteria
		 */
		if ((unsigned long)(now - curr->access_time) > wps_aplockdown.duration) {
			prev->next = NULL;
			wps_aplockdown.tail = prev;
			while (curr) {
				next = curr->next;
				free(curr);
				curr = next;
			}
			break;
		}

		wps_aplockdown.living_cnt++;
		prev = curr;
		curr = curr->next;
	}

	TUTRACE((TUTRACE_INFO, "Fail AP pin trial count = %d!\n", wps_aplockdown.living_cnt));

	return wps_aplockdown.locked;
}

int
wps_aplockdown_check(void)
{
	unsigned long now;
  int ageout;
  
	if (wps_aplockdown.enabled == 0 || wps_aplockdown.living_cnt == 0)
		return 0;

	/* get current time and oldest time */
	(void) time(&now);

	/* check if latest pinfail trial is ageout */
  ageout = wps_aplockdown.ageout;
  if (ageout < wps_aplockdown.init_ageout)  
    ageout = wps_aplockdown.init_ageout;
    	
  if ((unsigned long)(now - wps_aplockdown.head->access_time) > ageout) {
		/* clean up nodes within ap_lockdown_log */
		wps_aplockdown_cleanup();

		if (wps_aplockdown.locked) {
			/* unset apLockDown indicator */
			wps_aplockdown.locked = 0;

			wps_ui_set_env("wps_aplockdown", "0");
			wps_set_conf("wps_aplockdown", "0");

			/* reset the IE */
			wps_ie_set(NULL, NULL);

			TUTRACE((TUTRACE_INFO, "Unlock AP lock down\n"));
		}
	}

	return wps_aplockdown.locked;
}

int
wps_aplockdown_islocked()
{
	return wps_aplockdown.locked | wps_aplockdown.force_on;
}

int
wps_aplockdown_cleanup()
{
	WPS_APLOCKDOWN_NODE *curr = NULL;
	WPS_APLOCKDOWN_NODE *next = NULL;

	if (wps_aplockdown.enabled == 0)
		return 0;

	curr = wps_aplockdown.head;
	while (curr) {
		next = curr->next;
		free(curr);
		curr = next;
	}

	/* clear old info in ap_lockdown_log */
	wps_aplockdown.head = NULL;
	wps_aplockdown.tail = NULL;
	wps_aplockdown.living_cnt = 0;

	return 0;
}

void
wps_aplockdown_reset_ageout()
{
  wps_aplockdown.ageout = 0;
}

