/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsStringService.h"

#include "nsString.h"
#include "nsReadableUtils.h"

nsStringService::nsStringService()
{
}
nsStringService::~nsStringService()
{
}

NS_IMPL_ISUPPORTS1(nsStringService,
                   nsIStringService);


NS_IMETHODIMP 
nsStringService::CreateAString(const PRUnichar *aString, PRInt32 aLength, nsAString * *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsString* string = new nsString(aString, aLength);
  if (!string)
    return NS_ERROR_OUT_OF_MEMORY;
  
  *_retval = string;
  return NS_OK;
}

NS_IMETHODIMP 
nsStringService::CreateACString(const char *aString, PRInt32 aLength, nsACString * *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsCString* string = new nsCString(aString, aLength);
  if (!string)
    return NS_ERROR_OUT_OF_MEMORY;
  
  *_retval = string;
  return NS_OK;
}

NS_IMETHODIMP 
nsStringService::DeleteAString(nsAString * aString)
{
  delete aString;
  return NS_OK;
}

NS_IMETHODIMP 
nsStringService::DeleteACString(nsACString * aString)
{
  delete aString;
  return NS_OK;
}

NS_IMETHODIMP 
nsStringService::GetString(const nsACString & aString, char **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = ToNewCString(aString);
  if (!*_retval)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP 
nsStringService::GetWString(const nsAString & aString, PRUnichar **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = ToNewUnicode(aString);
  if (!*_retval)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

