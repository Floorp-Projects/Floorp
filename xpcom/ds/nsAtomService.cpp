/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

#include "nsAtomService.h"

NS_IMPL_ISUPPORTS1(nsAtomService, nsIAtomService)

nsAtomService::nsAtomService()
{}

nsAtomService::~nsAtomService()
{}


nsresult
nsAtomService::GetAtom(const PRUnichar *aString, nsIAtom ** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  
  *aResult = NS_NewAtom(aString);

  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  
  return NS_OK;
}
