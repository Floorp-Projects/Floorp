/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */


/*
   privacy.h --- prototypes for privacy policy functions.
   Created: David Meeker <dfm@netscape.com>, 09-Jun-98.
*/


#ifndef _PRIVACY_H
#define _PRIVACY_H

#include "ntypes.h"



XP_BEGIN_PROTOS

extern void
PRVCY_CheckStandardLocation(MWContext * context);

extern Bool 
PRVCY_CurrentHasPrivacyPolicy(MWContext * ctxt);

extern Bool
PRVCY_ExternalContextDescendant(MWContext *ctxt);

extern char *
PRVCY_GetCurrentPrivacyPolicyURL(MWContext * ctxt);

extern Bool 
PRVCY_PrivacyPolicyConfirmSubmit(MWContext *ctxt, 
                                 LO_FormElementStruct *form_element);

extern char *
PRVCY_TutorialURL();

extern void
PRVCY_ToggleAnonymous();

extern Bool
PRVCY_IsAnonymous();

extern void
PRVCY_SiteInfo(MWContext *context);

XP_END_PROTOS

#endif /* !_PRIVACY_H */
