/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsAgg.h"
#include "nsIProtocolConnection.h"
#include "nsIHttpUrl.h"
#include "nsIPostToServer.h"

 #include "nsINetService.h"  /* XXX: NS_FALSE */
#include "nsNetStream.h"
#include "net.h"

#include "prmem.h"
#include "plstr.h"

MWContext *new_stub_context(URL_Struct *URL_s);

static NS_DEFINE_IID(kIOutputStreamIID,  NS_IOUTPUTSTREAM_IID);


class nsHttpUrlImpl : public nsIProtocolConnection,
                      public nsIPostToServer,
                      public nsIHttpUrl
{
public:
    typedef enum {
        Send_None,
        Send_File,
        Send_Data,
        Send_DataFromFile,
    } SendType;

public:
    nsHttpUrlImpl(nsISupports* outer);

    /* Aggregated nsISupports interface... */
    NS_DECL_AGGREGATED

    /* nsIProtocolConnection interface... */
    NS_IMETHOD InitializeURLInfo(URL_Struct_ *URL_s);
    NS_IMETHOD GetURLInfo(URL_Struct_** aResult);

    /* nsIPostToServer interface... */
    NS_IMETHOD  SendFile(const char *aFile);
    NS_IMETHOD  SendData(const char *aBuffer, PRInt32 aLength);
    NS_IMETHOD  SendDataFromFile(const char *aFile);

    /* Handle http-equiv meta tags. */
    NS_IMETHOD  AddMimeHeader(const char *name, const char *value);

    /* Here's our link to the netlib world.... */
    URL_Struct *m_URL_s;

    /* nsIHttpUrl interface... */


static nsISupports* NewHttpUrlImpl(nsISupports *outer);

protected:
    virtual ~nsHttpUrlImpl();

    nsresult PostFile(const char *aFile);

    SendType m_PostType;
    char *m_PostBuffer;
    PRInt32 m_PostBufferLength;
};



nsHttpUrlImpl::nsHttpUrlImpl(nsISupports* outer)
{
    NS_INIT_AGGREGATED(outer);

    m_PostType = Send_None;
    m_PostBuffer = nsnull;
    m_PostBufferLength = 0;
    m_URL_s = nsnull;
}

nsHttpUrlImpl::~nsHttpUrlImpl()
{
    if (nsnull != m_PostBuffer) PR_Free(m_PostBuffer);
    if (nsnull != m_URL_s) {
        NET_DropURLStruct(m_URL_s);
    }
}



NS_IMPL_AGGREGATED(nsHttpUrlImpl)

nsresult nsHttpUrlImpl::AggregatedQueryInterface(const nsIID &aIID, 
                                             void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }

    static NS_DEFINE_IID(kISupportsIID,            NS_ISUPPORTS_IID);
    static NS_DEFINE_IID(kIProtocolConnectionIID,  NS_IPROTOCOLCONNECTION_IID);
    static NS_DEFINE_IID(kIPostToServerIID,        NS_IPOSTTOSERVER_IID);
    static NS_DEFINE_IID(kIHttpUrlIID,             NS_IHTTPURL_IID);
    if (aIID.Equals(kIProtocolConnectionIID)) {
        *aInstancePtr = (void*) ((nsIProtocolConnection*)this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kIHttpUrlIID)) {
        *aInstancePtr = (void*) ((nsIHttpUrl*)this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kIPostToServerIID)) {
        *aInstancePtr = (void*) ((nsIPostToServer*)this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsISupports *)&fAggregated);
        AddRef();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}


NS_METHOD nsHttpUrlImpl::InitializeURLInfo(URL_Struct *URL_s)
{
    nsresult result = NS_OK;

    /* Hook us up with the world. */
    m_URL_s = URL_s;
    NET_HoldURLStruct(URL_s);

    if (Send_None != m_PostType) {
        /* Free any existing POST data hanging off the URL_Struct */
        if (nsnull != URL_s->post_data) {
            PR_Free(URL_s->post_data);
        }

        /*
         * Store the new POST data into the URL_Struct
         */
        URL_s->post_data         = m_PostBuffer;
        URL_s->post_data_size    = m_PostBufferLength;
        URL_s->post_data_is_file = (Send_Data == m_PostType) ? FALSE : TRUE;
        /*
         * Is the request a POST or PUT
         */
        URL_s->method = (Send_File == m_PostType) ? URL_PUT_METHOD : URL_POST_METHOD;

        /* Reset the local post state... */
        m_PostType = Send_None;
        m_PostBuffer = nsnull;  /* The URL_Struct owns the memory now... */
        m_PostBufferLength = 0;
    }

    return result;
}


NS_METHOD nsHttpUrlImpl::GetURLInfo(URL_Struct_** aResult)
{
  nsresult rv;

  if (nsnull == aResult) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
    /* XXX: Should the URL be reference counted here?? */
    *aResult = m_URL_s;
    rv = NS_OK;
  }

  return rv;
}


NS_METHOD  nsHttpUrlImpl::SendFile(const char *aFile)
{
    nsresult result;

    result = PostFile(aFile);
    if (NS_OK == result) {
        m_PostType = Send_File;
    }
    return result;
}


NS_METHOD  nsHttpUrlImpl::SendDataFromFile(const char *aFile)
{
    nsresult result;

    result = PostFile(aFile);
    if (NS_OK == result) {
        m_PostType = Send_DataFromFile;
    }
    return result;
}


NS_METHOD  nsHttpUrlImpl::SendData(const char *aBuffer, PRInt32 aLength)
{
    nsresult result = NS_OK;

    /* Deal with error conditions... */
    if (nsnull == aBuffer) {
        result = NS_ERROR_ILLEGAL_VALUE;
        goto done;
    } 
    else if (Send_None != m_PostType) {
        result = NS_IPOSTTOSERVER_ALREADY_POSTING;
        goto done;
    }

    /* Copy the post data... */
    m_PostBuffer = (char *)PR_Malloc(aLength+1);
    if (nsnull == m_PostBuffer) {
        result = NS_ERROR_OUT_OF_MEMORY;
        goto done;
    }
    memcpy(m_PostBuffer, aBuffer, aLength);
    m_PostBuffer[aLength] = '\0';
    m_PostBufferLength = aLength;
    m_PostType = Send_Data;

done:
    return result;
}


NS_METHOD nsHttpUrlImpl::AddMimeHeader(const char *name, const char *value)
{
    MWContext *stubContext=NULL;
    char *aName=NULL;
    char *aVal=NULL;
    PRBool addColon=TRUE;
    PRInt32 len=0;

    NS_PRECONDITION((name != nsnull) && ((*name) != nsnull), "Bad name");
    NS_PRECONDITION((value != nsnull) && ((*value) != nsnull), "Bad value");
    
    if(!name
       || !*name
       || !value
       || !*value)
        return NS_FALSE;

    // Make sure we've got a colon on the end of the header name
    if(PL_strchr(name, ':'))
        addColon=FALSE;

    /* Bring in our own copies of the data. */
    if(addColon)
        aName = (char*)PR_Malloc(PL_strlen(name)+2); // add extra byte for colon
    else
        aName = (char*)PR_Malloc(PL_strlen(name)+1);
    if(!aName)
        return NS_ERROR_OUT_OF_MEMORY;
    aVal  = (char*)PR_Malloc(PL_strlen(value)+1);
    if(!aVal)
        return NS_ERROR_OUT_OF_MEMORY;

    PL_strcpy(aName, name);
    if(addColon) {
        PL_strcat(aName, ":");
    }
        
    PL_strcpy(aVal, value);

    stubContext = new_stub_context(m_URL_s);

    /* Make the real call to NET_ParseMimeHeader, passing in the dummy bam context. */


    NET_ParseMimeHeader(FO_CACHE_AND_NGLAYOUT,
				stubContext, 
				m_URL_s, 
				aName, 
				aVal,
				TRUE);
    PR_Free(aName);
    PR_Free(aVal);
    return NS_OK;
}

nsresult nsHttpUrlImpl::PostFile(const char *aFile)
{
    nsresult result = NS_OK;

    /* Deal with error conditions... */
    if (nsnull == aFile) {
        result = NS_ERROR_ILLEGAL_VALUE;
        goto done;
    } 
    else if (Send_None != m_PostType) {
        result = NS_IPOSTTOSERVER_ALREADY_POSTING;
        goto done;
    }

    /* Copy the post data... */
    m_PostBuffer = PL_strdup(aFile);

    if (nsnull == m_PostBuffer) {
        result = NS_ERROR_OUT_OF_MEMORY;
        goto done;
    }
    m_PostBufferLength = PL_strlen(aFile);

done:
    return result;
}


nsISupports* nsHttpUrlImpl::NewHttpUrlImpl(nsISupports* aOuter)
{
    nsHttpUrlImpl* it;
    nsISupports *result = nsnull;

    it = new nsHttpUrlImpl(aOuter);
    if (nsnull != it) {
        if (nsnull != aOuter) {
            result = &it->fAggregated;
        } else {
            result = (nsIProtocolConnection *)it;
        }

        result->AddRef();
    }

    return result;
}



extern "C" NS_NET nsresult NS_NewHttpUrl(nsISupports** aInstancePtrResult,
                                         nsISupports* aOuter)
{
    NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
    if (nsnull == aInstancePtrResult) {
        return NS_ERROR_NULL_POINTER;
    }

    *aInstancePtrResult = nsHttpUrlImpl::NewHttpUrlImpl(aOuter);
    if (nsnull == *aInstancePtrResult) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}

