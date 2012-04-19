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
 * $Id: wps_aplockdown.h,v 1.1 2009-07-29 06:44:31 Exp $
 */

#ifndef __WPS_APLOCKDOWN_H__
#define __WPS_APLOCKDOWN_H__

int wps_aplockdown_init();
int wps_aplockdown_add(void);
int wps_aplockdown_check(void);
int wps_aplockdown_islocked();
int wps_aplockdown_cleanup();
void wps_aplockdown_reset_ageout();
#endif	/* __WPS_APLOCKDOWN_H__ */
