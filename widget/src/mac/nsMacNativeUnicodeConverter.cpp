/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * is Netscape Communications Corporation.
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

#include "nsMemory.h"
#include "nsMacNativeUnicodeConverter.h"

#include <TextUtils.h>
#include <UnicodeConverter.h>
#include <Fonts.h>

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsresult nsMacNativeUnicodeConverter::ConvertScripttoUnicode(ScriptCode aScriptCode, 
                                                             const char *aMultibyteStr,
                                                             PRInt32 aMultibyteStrLen,
                                                             PRUnichar **aUnicodeStr,
                                                             PRInt32 *aUnicodeStrLen)
{
  NS_ENSURE_ARG(aUnicodeStr);
  NS_ENSURE_ARG(aMultibyteStr);

  *aUnicodeStr = nsnull;
  *aUnicodeStrLen = 0;
	
  TextEncoding textEncodingFromScript;
  OSErr err = ::UpgradeScriptInfoToTextEncoding(aScriptCode, 
                                                kTextLanguageDontCare, 
                                                kTextRegionDontCare, 
                                                nsnull,
                                                &textEncodingFromScript);
  NS_ENSURE_TRUE(err == noErr, NS_ERROR_FAILURE);
	
  TextToUnicodeInfo	textToUnicodeInfo;
  err = ::CreateTextToUnicodeInfoByEncoding(textEncodingFromScript, &textToUnicodeInfo);
  NS_ENSURE_TRUE(err == noErr, NS_ERROR_FAILURE);

  // for MacArabic and MacHebrew, the corresponding Unicode string could be up to
  // six times as big.
  UInt32 factor;
  if (aScriptCode == smArabic ||
      aScriptCode == smHebrew ||
      aScriptCode == smExtArabic)
    factor = 6;
  else
    factor = 2;
    
  UniChar *unicodeStr = (UniChar *) nsMemory::Alloc(aMultibyteStrLen * factor);
  if (!unicodeStr) {
    ::DisposeTextToUnicodeInfo(&textToUnicodeInfo);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  ByteCount sourceRead;
  ByteCount unicodeLen;
  
  err = ::ConvertFromTextToUnicode(textToUnicodeInfo,
                                   aMultibyteStrLen,
                                   (ConstLogicalAddress) aMultibyteStr,
                                   kUnicodeUseFallbacksMask | 
                                   kUnicodeLooseMappingsMask,
                                   0,nsnull,nsnull,nsnull,
                                   aMultibyteStrLen * factor,
                                   &sourceRead,
                                   &unicodeLen,
                                   unicodeStr);
				
  if (err == noErr)
  {
    *aUnicodeStr = (PRUnichar *) unicodeStr;
    *aUnicodeStrLen = unicodeLen / sizeof(UniChar); // byte count -> char count
  }
          									
  ::DisposeTextToUnicodeInfo(&textToUnicodeInfo);
  										                          
  return err == noErr ? NS_OK : NS_ERROR_FAILURE;                                  
}

nsresult nsMacNativeUnicodeConverter::ConvertUnicodetoScript(const PRUnichar *aUnicodeStr, 
                                                             PRInt32 aUnicodeStrLen,
                                                             char **aMultibyteStr,
                                                             PRInt32 *aMultibyteStrlen,
                                                             ScriptCodeRun **aScriptCodeRuns,
                                                             PRInt32 *aScriptCodeRunLen)
{
  NS_ENSURE_ARG(aUnicodeStr);
  NS_ENSURE_ARG(aMultibyteStr);
  NS_ENSURE_ARG(aMultibyteStrlen);
  NS_ENSURE_ARG(aScriptCodeRuns);
  NS_ENSURE_ARG(aScriptCodeRunLen);
  
  *aMultibyteStr = nsnull;
  *aMultibyteStrlen = 0;
  *aScriptCodeRuns = nsnull;
  *aScriptCodeRunLen = 0;
  
  // Get a list of installed script.
  ItemCount numberOfScriptCodes = ::GetScriptManagerVariable(smEnabled);
  ScriptCode *scriptArray = (ScriptCode *) nsMemory::Alloc(sizeof(ScriptCode) * numberOfScriptCodes);
  NS_ENSURE_TRUE(scriptArray, NS_ERROR_OUT_OF_MEMORY);
	
  for (ScriptCode i = 0, j = 0; i <= smUninterp && j < numberOfScriptCodes; i++)
  {
    if (::GetScriptVariable(i, smScriptEnabled))
      scriptArray[j++] = i;
  }

  OSErr err;
  UnicodeToTextRunInfo unicodeToTextInfo;
  err = ::CreateUnicodeToTextRunInfoByScriptCode(numberOfScriptCodes,
                                                 scriptArray,
                                                 &unicodeToTextInfo); 
  nsMemory::Free(scriptArray);                                                 
  NS_ENSURE_TRUE(err == noErr, NS_ERROR_FAILURE);
                                              
  ByteCount inputRead;
  ItemCount scriptRunOutLen;
  ScriptCodeRun *scriptCodeRuns = NS_REINTERPRET_CAST(ScriptCodeRun*,
                                                      nsMemory::Alloc(sizeof(ScriptCodeRun) * 
                                                      aUnicodeStrLen));
  if (!scriptCodeRuns) {
    ::DisposeUnicodeToTextRunInfo(&unicodeToTextInfo);
    return NS_ERROR_OUT_OF_MEMORY;
  }
	
  // Allocate 2 bytes per character. 
  // I think that would be enough for charset encodings used by legacy Macintosh apps.
  LogicalAddress outputStr = (LogicalAddress) nsMemory::Alloc(aUnicodeStrLen * 2);
  if (!outputStr)
  {
    ::DisposeUnicodeToTextRunInfo(&unicodeToTextInfo);
    nsMemory::Free(scriptCodeRuns);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  ByteCount outputLen = 0;

  err = ::ConvertFromUnicodeToScriptCodeRun(unicodeToTextInfo,
                                            aUnicodeStrLen * sizeof(UniChar),
                                            (UniChar *) aUnicodeStr,
                                            kUnicodeUseFallbacksMask | 
                                            kUnicodeLooseMappingsMask |
                                            kUnicodeTextRunMask,
                                            0,
                                            nsnull,
                                            nsnull,
                                            nsnull,
                                            aUnicodeStrLen * sizeof(PRUnichar), // char count -> byte count
                                            &inputRead,
                                            &outputLen,
                                            outputStr,
                                            aUnicodeStrLen,
                                            &scriptRunOutLen,
                                            scriptCodeRuns);
                                                   
  if (outputLen > 0 &&
      (err == noErr ||
       err == kTECUnmappableElementErr ||
       err == kTECOutputBufferFullStatus))
  {
    // set output as long as something got converted
    // since this function is called as a fallback
    *aMultibyteStr = (char *) outputStr;
    *aMultibyteStrlen = outputLen;
    *aScriptCodeRuns = scriptCodeRuns;
    *aScriptCodeRunLen = scriptRunOutLen;
    err = noErr;
  }

  ::DisposeUnicodeToTextRunInfo(&unicodeToTextInfo);
  
  return err == noErr ? NS_OK : NS_ERROR_FAILURE;                                  
}
	
