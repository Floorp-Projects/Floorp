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
 */

#include "nsIScriptError.h"
#include "nsString.h"

class nsScriptError : public nsIScriptError {
public:
    nsScriptError();

    virtual ~nsScriptError();

  // TODO - do something reasonable on getting null from these babies.

    nsScriptError(const PRUnichar *message,
                  const PRUnichar *sourceName, // or URL
                  const PRUnichar *sourceLine,
                  PRUint32 lineNumber,
                  PRUint32 columnNumber,
                  PRUint32 flags,
                  const char *category)
    {
        mMessage.SetString(message);
        mSourceName.SetString(sourceName);
        mLineNumber = lineNumber;
        mSourceLine.SetString(sourceLine);
        mColumnNumber = columnNumber;
        mFlags = flags;
        mCategory.SetString(category);
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSICONSOLEMESSAGE
    NS_DECL_NSISCRIPTERROR

private:
    nsAutoString mMessage;
    nsAutoString mSourceName;
    PRUint32 mLineNumber;
    nsAutoString mSourceLine;
    PRUint32 mColumnNumber;
    PRUint32 mFlags;
    nsCAutoString mCategory;
};


