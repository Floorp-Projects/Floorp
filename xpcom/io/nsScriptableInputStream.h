/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Contributor(s): Jud Valeski & Brendan Eich
 */

#ifndef ___nsscriptableinputstream___h_
#define ___nsscriptableinputstream___h_

#include "nsIScriptableInputStream.h"
#include "nsIInputStream.h"
#include "nsCOMPtr.h"

#define NS_SCRIPTABLEINPUTSTREAM_CID        \
{ 0x7225c040, 0xa9bf, 0x11d3, { 0xa1, 0x97, 0x0, 0x50, 0x4, 0x1c, 0xaf, 0x44 } }

#define NS_SCRIPTABLEINPUTSTREAM_CONTRACTID "@mozilla.org/scriptableinputstream;1"
#define NS_SCRIPTABLEINPUTSTREAM_CLASSNAME "Scriptable Input Stream"

class nsScriptableInputStream : public nsIScriptableInputStream {
public:
    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIScriptableInputStream methods
    NS_DECL_NSISCRIPTABLEINPUTSTREAM

    // nsScriptableInputStream methods
    nsScriptableInputStream() { NS_INIT_REFCNT(); };
    virtual ~nsScriptableInputStream() {};

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);
private:
    nsCOMPtr<nsIInputStream> mInputStream;
};

#endif // ___nsscriptableinputstream___h_
