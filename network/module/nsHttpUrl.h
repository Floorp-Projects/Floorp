/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsHttpUrl_h__
#define nsHttpUrl_h__

#include "nsIURL.h"
#include "nsINetlibURL.h"
#include "nsIHttpURL.h"
#include "nsIPostToServer.h"
#include "nsCOMPtr.h"

class nsHttpUrlImpl : public nsIURL, public nsINetlibURL, public nsIHttpURL,
                      public nsIPostToServer    // XXX for now
{
public:
    // from nsIURL:

    NS_IMETHOD_(PRBool) Equals(const nsIURL *aURL) const;
    NS_IMETHOD GetSpec(const char* *result) const;
    NS_IMETHOD SetSpec(const char* spec);
    NS_IMETHOD GetProtocol(const char* *result) const;
    NS_IMETHOD SetProtocol(const char* protocol);
    NS_IMETHOD GetHost(const char* *result) const;
    NS_IMETHOD SetHost(const char* host);
    NS_IMETHOD GetHostPort(PRUint32 *result) const;
    NS_IMETHOD SetHostPort(PRUint32 port);
    NS_IMETHOD GetFile(const char* *result) const;
    NS_IMETHOD SetFile(const char* file);
    NS_IMETHOD GetRef(const char* *result) const;
    NS_IMETHOD SetRef(const char* ref);
    NS_IMETHOD GetSearch(const char* *result) const;
    NS_IMETHOD SetSearch(const char* search);
    NS_IMETHOD GetContainer(nsISupports* *result) const;
    NS_IMETHOD SetContainer(nsISupports* container);
    NS_IMETHOD GetLoadAttribs(nsILoadAttribs* *result) const;
    NS_IMETHOD SetLoadAttribs(nsILoadAttribs* loadAttribs);
    NS_IMETHOD GetURLGroup(nsIURLGroup* *result) const;
    NS_IMETHOD SetURLGroup(nsIURLGroup* group);
    NS_IMETHOD SetPostHeader(const char* name, const char* value);
    NS_IMETHOD SetPostData(nsIInputStream* input);
    NS_IMETHOD GetContentLength(PRInt32 *len);
    NS_IMETHOD GetServerStatus(PRInt32 *status);
    NS_IMETHOD ToString(PRUnichar* *aString) const;
  
    // from nsINetlibURL:

    NS_IMETHOD GetURLInfo(URL_Struct_ **aResult) const;
    NS_IMETHOD SetURLInfo(URL_Struct_ *URL_s);

    // nsHttpUrlImpl:

    typedef enum {
        Send_None,
        Send_File,
        Send_Data,
        Send_DataFromFile
    } SendType;

    nsHttpUrlImpl(nsISupports* aContainer, nsIURLGroup* aGroup);

    NS_DECL_ISUPPORTS

    /* nsIPostToServer interface... */
    NS_IMETHOD  SendFile(const char *aFile);
    NS_IMETHOD  SendData(const char *aBuffer, PRUint32 aLength);
    NS_IMETHOD  SendDataFromFile(const char *aFile);

    /* nsIHttpUrl interface... */

    /* Handle http-equiv meta tags. */
    NS_IMETHOD  AddMimeHeader(const char *name, const char *value);

    nsresult ParseURL(const nsString& aSpec, const nsIURL* aURL = nsnull);

protected:
    virtual ~nsHttpUrlImpl();

    nsresult PostFile(const char *aFile);
    void ReconstructSpec(void);

    /* Here's our link to the netlib world.... */
    URL_Struct *m_URL_s;

    SendType m_PostType;
    char *m_PostBuffer;
    PRInt32 m_PostBufferLength;

    char* mSpec;
    char* mProtocol;
    char* mHost;
    char* mFile;
    char* mRef;
    char* mSearch;
    PRInt32 mPort;

    nsCOMPtr<nsIInputStream> mPostData;
    nsISupports*             mContainer;    // explicitly changed to no longer own its container
    nsCOMPtr<nsILoadAttribs> mLoadAttribs;
    nsIURLGroup*             mURLGroup;     // explicitly changed to no longer own its group
};

#endif // nsHttpUrl_h__
