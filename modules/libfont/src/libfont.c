/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* 
 *
 * General include that contain all general junk that is in the system.
 *
 * dp Suresh <dp@netscape.com>
 */


#include "libfont.h"
#include "nf.h"

/*
 * Global variables
 */

/*
 * These are the font broker interface variables that is available after
 * font broker init.
 */
struct nffbc *WF_fbc = NULL;
struct nffbu *WF_fbu = NULL;
struct nffbp *WF_fbp = NULL;

/*
 * JMC defines a lot of IDs. We need to define globals for all those here.
 * Everybody else will use these.
 *
 * Defining this causes JMC to generate the actual IDs for the objects.
 * JMC should have defined this automatically. Since work on JMC has
 * stopped, we will side step this by defining this ourselves here.
 */
#define JMC_INITID

/* MAKE ABSOLUTELY CERTAIN THAT jmc.h IS NOT INCLUDE BEFORE THIS POINT */
#ifdef JMC_H
jmc.h should not have been include here.
#endif

#define JMC_INIT_nfdoer_ID
#include "Mnfdoer.h"
#define JMC_INIT_nff_ID
#include "Mnff.h"
#define JMC_INIT_nffbc_ID
#include "Mnffbc.h"
#define JMC_INIT_nffbp_ID
#include "Mnffbp.h"
#define JMC_INIT_nffbu_ID
#include "Mnffbu.h"
#define JMC_INIT_nffmi_ID
#include "Mnffmi.h"
#define JMC_INIT_nffp_ID
#include "Mnffp.h"
#define JMC_INIT_nfrc_ID
#include "Mnfrc.h"
#define JMC_INIT_nfrf_ID
#include "Mnfrf.h"
#define JMC_INIT_nfstrm_ID
#include "Mnfstrm.h"
#define JMC_INIT_nfdlm_ID
#include "Mnfdlm.h"

