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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Frank Tang <ftang@netsape.com>
 */
#include "nsGtkIMEHelper.h"
#include "nsIUnicodeDecoder.h"
#include "nsIPlatformCharset.h"
#include "nsICharsetConverterManager.h"
#include "nsIServiceManager.h"
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

nsGtkIMEHelper *nsGtkIMEHelper::gSingleton = nsnull;
nsGtkIMEHelper* nsGtkIMEHelper::GetSingleton()
{
  if(! gSingleton)
    gSingleton = new nsGtkIMEHelper();
  NS_ASSERTION(gSingleton, "do not have singleton");
  return gSingleton;  
}
//-----------------------------------------------------------------------
nsGtkIMEHelper::nsGtkIMEHelper()
{
  SetupUnicodeDecoder(); 
}
nsGtkIMEHelper::~nsGtkIMEHelper()
{
  NS_IF_RELEASE(mDecoder);
}
//-----------------------------------------------------------------------
nsresult
nsGtkIMEHelper::ConvertToUnicode( const char* aSrc, PRInt32* aSrcLen,
  PRUnichar* aDest, PRInt32* aDestLen)
{
  NS_ASSERTION(mDecoder, "do not have mDecoder");
  if(! mDecoder)
     return NS_ERROR_ABORT;
  return mDecoder->Convert(aSrc, aSrcLen, aDest, aDestLen);
}

//-----------------------------------------------------------------------
void nsGtkIMEHelper::SetupUnicodeDecoder()
{
  mDecoder = nsnull;
  nsresult result = NS_ERROR_FAILURE;
  NS_WITH_SERVICE(nsIPlatformCharset, platform, NS_PLATFORMCHARSET_PROGID,
                  &result);
  if (platform && NS_SUCCEEDED(result)) {
    nsAutoString charset("");
    result = platform->GetCharset(kPlatformCharsetSel_Menu, charset);
    if (NS_FAILED(result) || (charset.Length() == 0)) {
      charset = "ISO-8859-1";   // default
    }
    nsICharsetConverterManager* manager = nsnull;
    nsresult res = nsServiceManager::
      GetService(kCharsetConverterManagerCID,
                 nsCOMTypeInfo<nsICharsetConverterManager>::GetIID(),
                 (nsISupports**)&manager);
    if (manager && NS_SUCCEEDED(res)) {
      manager->GetUnicodeDecoder(&charset, &mDecoder);
      nsServiceManager::ReleaseService(kCharsetConverterManagerCID, manager);
    }
  }
  NS_ASSERTION(mDecoder, "cannot get decoder");
}

