 /* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
 
 
#ifndef _nsINetSupportDialog_h__
#define _nsINetSupportDialog_h__

class nsIFactory;

#include "nsIPrompt.h"

// {05650684-eb9f-11d2-8e19-9ac64aca4d3c}
#define NS_NETSUPPORTDIALOG_CID \
{ 0x05650684, 0xeb9f, 0x11d2, { 0x8e, 0x19, 0x9a, 0xc6, 0x4a, 0xca, 0x4d, 0x3c } }

nsresult NS_NewNetSupportDialogFactory(nsIFactory** result);

#endif

