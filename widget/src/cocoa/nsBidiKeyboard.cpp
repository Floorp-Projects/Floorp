/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is IBM code.
 * 
 * The Initial Developer of the Original Code is IBM.
 * Portions created by IBM are
 * Copyright (C) International Business Machines
 * Corporation, 2000.  All Rights Reserved.
 * 
 * Contributor(s): Simon Montagu
 */

#include "nsBidiKeyboard.h"

NS_IMPL_ISUPPORTS1(nsBidiKeyboard, nsIBidiKeyboard)

nsBidiKeyboard::nsBidiKeyboard() : nsIBidiKeyboard()
{
  NS_INIT_REFCNT();
}

nsBidiKeyboard::~nsBidiKeyboard()
{
}

NS_IMETHODIMP nsBidiKeyboard::IsLangRTL(PRBool *aIsRTL)
{
  *aIsRTL = PR_FALSE;
#ifdef IBMBIDI
  // XXX Insert platform specific code to determine keyboard direction
#endif
  return NS_OK;
}

NS_IMETHODIMP nsBidiKeyboard::SetLangFromBidiLevel(PRUint8 aLevel)
{
#ifdef IBMBIDI
  // XXX Insert platform specific code to set keyboard language
#endif
  return NS_OK;
}
