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
   receipt.h --- prototypes for receipt-like things.
   Created: David Meeker <dfm@netscape.com>, 09-Jun-98.
 */  

#ifndef _RECEIPT_H
#define _RECEIPT_H

#include "ntypes.h"

#define RECEIPT_CONTAINER_URL_PREFIX "receipt-container:"
#define RECEIPT_URL_PREFIX "receipt:"

XP_BEGIN_PROTOS

extern void
RT_SaveDocument (MWContext *context, LO_FormElementStruct *form_element);

XP_END_PROTOS

#endif /* !_RECEIPT_H */








