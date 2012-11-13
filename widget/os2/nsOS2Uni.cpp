/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsOS2Uni.h"
#include "nsIServiceManager.h"
#include "nsIPlatformCharset.h"
#include <stdlib.h>


/**********************************************************
    OS2Uni
 **********************************************************/
nsICharsetConverterManager* OS2Uni::gCharsetManager = nullptr;

struct ConverterInfo
{
  uint16_t            mCodePage;
  const char*         mConvName;
  nsIUnicodeEncoder*  mEncoder;
  nsIUnicodeDecoder*  mDecoder;
};

#define eCONVERTER_COUNT  17
ConverterInfo gConverterInfo[eCONVERTER_COUNT] =
{
  { 0,    "",              nullptr,  nullptr },
  { 1252, "windows-1252",  nullptr,  nullptr },
  { 1208, "UTF-8",         nullptr,  nullptr },
  { 1250, "windows-1250",  nullptr,  nullptr },
  { 1251, "windows-1251",  nullptr,  nullptr },
  { 813,  "ISO-8859-7",    nullptr,  nullptr },
  { 1254, "windows-1254",  nullptr,  nullptr },
  { 864,  "IBM864",        nullptr,  nullptr },
  { 1257, "windows-1257",  nullptr,  nullptr },
  { 874,  "windows-874",   nullptr,  nullptr },
  { 932,  "Shift_JIS",     nullptr,  nullptr },
  { 943,  "Shift_JIS",     nullptr,  nullptr },
  { 1381, "GB2312",        nullptr,  nullptr },
  { 1386, "GB2312",        nullptr,  nullptr },
  { 949,  "EUC-KR",        nullptr,  nullptr },
  { 950,  "Big5",          nullptr,  nullptr },
  { 1361, "x-johab",       nullptr,  nullptr }
};

nsISupports*
OS2Uni::GetUconvObject(int aCodePage, ConverterRequest aReq)
{
  if (gCharsetManager == nullptr) {
    CallGetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &gCharsetManager);
  }

  nsresult rv;
  nsISupports* uco = nullptr;
  for (int i = 0; i < eCONVERTER_COUNT; i++) {
    if (aCodePage == gConverterInfo[i].mCodePage) {
      if (gConverterInfo[i].mEncoder == nullptr) {
        const char* convname;
        nsAutoCString charset;
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
                                           nullptr, '?');
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
                    int32_t aSrcLength, nsAutoCharBuffer& aResult,
                    int32_t& aResultLength)
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
                    int32_t aSrcLength, nsAutoChar16Buffer& aResult,
                    int32_t& aResultLength)
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
