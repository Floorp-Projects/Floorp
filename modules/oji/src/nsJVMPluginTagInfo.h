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

#ifndef nsJVMPluginTagInfo_h___
#define nsJVMPluginTagInfo_h___

#include "nsIJVMPluginTagInfo.h"
#include "nsAgg.h"

/*******************************************************************************
 * nsJVMPluginTagInfo: The browser makes one of these when it sees an APPLET or
 * appropriate OBJECT tag.
 ******************************************************************************/

class nsIPluginTagInfo2;

class nsJVMPluginTagInfo : public nsIJVMPluginTagInfo {
public:

    NS_DECL_AGGREGATED

    /* from nsIJVMPluginTagInfo: */

    /* ====> These are usually only called by the plugin, not the browser... */

    NS_IMETHOD
    GetCode(const char* *result);

    NS_IMETHOD
    GetCodeBase(const char* *result);

    NS_IMETHOD
    GetArchive(const char* *result);

    NS_IMETHOD
    GetName(const char* *result);

    NS_IMETHOD
    GetMayScript(PRBool *result);

    /* Methods specific to nsJVMPluginInstancePeer: */
    
    /* ====> From here on are things only called by the browser, not the plugin... */

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr,
           nsIPluginTagInfo2* info);

protected:

    nsJVMPluginTagInfo(nsISupports* outer, nsIPluginTagInfo2* info);
    virtual ~nsJVMPluginTagInfo(void);

    /* Instance Variables: */
    nsIPluginTagInfo2*  fPluginTagInfo;
    char*               fSimulatedCodebase;
    char*               fSimulatedCode;
};

#endif // nsJVMPluginTagInfo_h___
