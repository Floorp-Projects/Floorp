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

#include "nsStylClipboardUtils.h"
#include "nsMemory.h"
#include "nsCarbonHelpers.h"

#include <Appearance.h>

nsresult CreateStylFromScriptRuns(ScriptCodeRun *scriptCodeRuns,
                                  ItemCount scriptRunOutLen,
                                  char **stylData,
                                  PRInt32 *stylLen)
{
  PRInt32 scrpRecLen = sizeof(short) + sizeof(ScrpSTElement) * scriptRunOutLen;
  StScrpRec *scrpRec = NS_REINTERPRET_CAST(StScrpRec*, 
                                           nsMemory::Alloc(scrpRecLen));
  NS_ENSURE_TRUE(scrpRec, NS_ERROR_OUT_OF_MEMORY);
  
  OSErr err = noErr;
#if TARGET_CARBON    
  Str255 themeFontName;
#endif  
  SInt16 textSize;
  Style textStyle;
  short fontFamilyID;
  FontInfo fontInfo;
  RGBColor textColor;
  textColor.red = textColor.green = textColor.blue = 0;
  
  // save font settings
  CGrafPtr curPort;
  short saveFontFamilyID;
  SInt16 saveTextSize;
  Style saveTextStyle;
  ::GetPort((GrafPtr*)&curPort);
  saveFontFamilyID = ::GetPortTextFont(curPort);
  saveTextSize = ::GetPortTextSize(curPort);
  saveTextStyle = ::GetPortTextFace(curPort);
  
  scrpRec->scrpNStyles = scriptRunOutLen;
  for (ItemCount i = 0; i < scriptRunOutLen; i++) {
    scrpRec->scrpStyleTab[i].scrpStartChar = scriptCodeRuns[i].offset;

#if TARGET_CARBON    
    err = ::GetThemeFont(
                         kThemeApplicationFont, 
                         scriptCodeRuns[i].script, 
                         themeFontName, 
                         &textSize, 
                         &textStyle);
    if (err != noErr)
      break;
      
    ::GetFNum(themeFontName, &fontFamilyID);
#else
    // kThemeApplicationFont cannot be used on MacOS 9
    // use script manager to get the application font instead
    fontFamilyID = NS_STATIC_CAST(short,
                                  ::GetScriptVariable(scriptCodeRuns[i].script, smScriptAppFond));
    textSize = NS_STATIC_CAST(SInt16,
                              ::GetScriptVariable(scriptCodeRuns[i].script, smScriptAppFondSize));
    textStyle = normal;
#endif
      
    ::TextFont(fontFamilyID);
    ::TextSize(textSize);
    ::TextFace(textStyle);
    ::GetFontInfo(&fontInfo);
    
    scrpRec->scrpStyleTab[i].scrpFont = fontFamilyID;
    scrpRec->scrpStyleTab[i].scrpHeight = fontInfo.ascent +
                                          fontInfo.descent +
                                          fontInfo.leading;
    scrpRec->scrpStyleTab[i].scrpAscent = fontInfo.ascent;
    scrpRec->scrpStyleTab[i].scrpFace = textStyle;
    scrpRec->scrpStyleTab[i].scrpSize = textSize;
    scrpRec->scrpStyleTab[i].scrpColor = textColor;
  }
       
  // restore font settings
  ::TextFont(saveFontFamilyID);
  ::TextSize(saveTextSize);
  ::TextFace(saveTextStyle);
  
  if (err != noErr) {
    nsMemory::Free(scrpRec);
    return NS_ERROR_FAILURE;
  }

  *stylData = NS_REINTERPRET_CAST(char*, scrpRec);
  *stylLen = scrpRecLen;
         
  return NS_OK;
}
