/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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

