/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is an API for accessing OS/2 Unicode support.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
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

#include "nsOS2Uni.h"
#include "nsIServiceManager.h"
#include "nsIPlatformCharset.h"
#include <stdlib.h>


/**********************************************************
    OS2Uni
 **********************************************************/
nsICharsetConverterManager* OS2Uni::gCharsetManager = nsnull;

struct ConverterInfo
{
  PRUint16            mCodePage;
  char*               mConvName;
  nsIUnicodeEncoder*  mEncoder;
  nsIUnicodeDecoder*  mDecoder;
};

#define eCONVERTER_COUNT  17
ConverterInfo gConverterInfo[eCONVERTER_COUNT] =
{
  { 0,    "",              nsnull,  nsnull },
  { 1252, "windows-1252",  nsnull,  nsnull },
  { 1208, "UTF-8",         nsnull,  nsnull },
  { 1250, "windows-1250",  nsnull,  nsnull },
  { 1251, "windows-1251",  nsnull,  nsnull },
  { 813,  "ISO-8859-7",    nsnull,  nsnull },
  { 1254, "windows-1254",  nsnull,  nsnull },
  { 864,  "IBM864",        nsnull,  nsnull },
  { 1257, "windows-1257",  nsnull,  nsnull },
  { 874,  "windows-874",   nsnull,  nsnull },
  { 932,  "Shift_JIS",     nsnull,  nsnull },
  { 943,  "Shift_JIS",     nsnull,  nsnull },
  { 1381, "GB2312",        nsnull,  nsnull },
  { 1386, "GB2312",        nsnull,  nsnull },
  { 949,  "x-windows-949", nsnull,  nsnull },
  { 950,  "Big5",          nsnull,  nsnull },
  { 1361, "x-johab",       nsnull,  nsnull }
};

nsISupports*
OS2Uni::GetUconvObject(int aCodePage, ConverterRequest aReq)
{
  if (gCharsetManager == nsnull) {
    CallGetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &gCharsetManager);
  }

  nsresult rv;
  nsISupports* uco = nsnull;
  for (int i = 0; i < eCONVERTER_COUNT; i++) {
    if (aCodePage == gConverterInfo[i].mCodePage) {
      if (gConverterInfo[i].mEncoder == nsnull) {
        const char* convname;
        nsCAutoString charset;
        if (aCodePage == 0) {
          nsCOMPtr<nsIPlatformCharset>
                      plat(do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv));
          if (NS_SUCCEEDED(rv)) {
            plat->GetCharset(kPlatformCharsetSel_FileName, charset);
          } else {
            // default to IBM850 if this should fail
            charset = "IBM850";
          }
          convname = charset.get();
        } else {
          convname = gConverterInfo[i].mConvName;
        }
        rv = gCharsetManager->GetUnicodeEncoderRaw(convname,
                                                   &gConverterInfo[i].mEncoder);
        gConverterInfo[i].mEncoder->
                    SetOutputErrorBehavior(nsIUnicodeEncoder::kOnError_Replace,
                                           nsnull, '?');
        gCharsetManager->GetUnicodeDecoderRaw(convname,
                                              &gConverterInfo[i].mDecoder);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to get converter");
      }
      if (aReq == eConv_Encoder) {
        uco = gConverterInfo[i].mEncoder;
      } else {
        uco = gConverterInfo[i].mDecoder;
      }
      break;
    }
  }

  return uco;
}

void OS2Uni::FreeUconvObjects()
{
  for (int i = 0; i < eCONVERTER_COUNT; i++) {
    NS_IF_RELEASE(gConverterInfo[i].mEncoder);
    NS_IF_RELEASE(gConverterInfo[i].mDecoder);
  }
  NS_IF_RELEASE(gCharsetManager);
}

/**********************************************************
    WideCharToMultiByte
 **********************************************************/
nsresult
WideCharToMultiByte(int aCodePage, const PRUnichar* aSrc,
                    PRInt32 aSrcLength, nsAutoCharBuffer& aResult,
                    PRInt32& aResultLength)
{
  nsresult rv;
  nsISupports* sup = OS2Uni::GetUconvObject(aCodePage, eConv_Encoder);
  nsCOMPtr<nsIUnicodeEncoder> uco = do_QueryInterface(sup);

  if (NS_FAILED(uco->GetMaxLength(aSrc, aSrcLength, &aResultLength))) {
    return NS_ERROR_UNEXPECTED;
  }
  if (!aResult.SetLength(aResultLength + 1))
    return NS_ERROR_OUT_OF_MEMORY;
  char* str = aResult.Elements();

  rv = uco->Convert(aSrc, &aSrcLength, str, &aResultLength);
  aResult[aResultLength] = '\0';
  return rv;
}

/**********************************************************
    MultiByteToWideChar
 **********************************************************/
nsresult
MultiByteToWideChar(int aCodePage, const char* aSrc,
                    PRInt32 aSrcLength, nsAutoChar16Buffer& aResult,
                    PRInt32& aResultLength)
{
  nsresult rv;
  nsISupports* sup = OS2Uni::GetUconvObject(aCodePage, eConv_Decoder);
  nsCOMPtr<nsIUnicodeDecoder> uco = do_QueryInterface(sup);

  if (NS_FAILED(uco->GetMaxLength(aSrc, aSrcLength, &aResultLength))) {
    return NS_ERROR_UNEXPECTED;
  }
  if (!aResult.SetLength(aResultLength + 1))
    return NS_ERROR_OUT_OF_MEMORY;
  PRUnichar* str = aResult.Elements();

  rv = uco->Convert(aSrc, &aSrcLength, str, &aResultLength);
  aResult[aResultLength] = '\0';
  return rv;
}
