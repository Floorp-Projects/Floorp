/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
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
 * The Initial Developer of the Original Code is is Netscape
 * Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMacNativeUnicodeConverter_h___
#define nsMacNativeUnicodeConverter_h___

#include "prtypes.h"
#include "nsError.h"
#include "nscore.h"
#include <MacTypes.h>

class nsISupports;


class nsMacNativeUnicodeConverter
{
public:

  // utitily functions to convert between native script and Unicode
  static nsresult ConvertScripttoUnicode(ScriptCode aScriptCode, 
                                         const char *aMultibyteStr,
                                         PRInt32 aMultibyteStrLen,
                                         PRUnichar **aUnicodeStr,
                                         PRInt32 *aUnicodeStrLen);
                                         
  static nsresult ConvertUnicodetoScript(const PRUnichar *aUnicodeStr, 
                                         PRInt32 aUnicodeStrLen,
                                         char **aMultibyteStr,
                                         PRInt32 *aMultibyteStrlen,
                                         ScriptCodeRun **aScriptCodeRuns,
                                         PRInt32 *aScriptCodeRunLen);

};


#endif // nsMacNativeUnicodeConverter_h___
