/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "if.h"
#include "ilINetReader.h"
#include "nsCRT.h"

class NetReaderImpl : public ilINetReader {
public:

  NetReaderImpl(il_container *aContainer);
  virtual ~NetReaderImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD WriteReady(PRUint32 *max_read);
  
  NS_IMETHOD FirstWrite(const unsigned char *str, int32 len, char* url);

  NS_IMETHOD Write(const unsigned char *str, int32 len);

  NS_IMETHOD StreamAbort(int status);

  NS_IMETHOD StreamComplete(PRBool is_multipart);

  NS_IMETHOD NetRequestDone(ilIURL *urls, int status);
  
  virtual PRBool StreamCreated(ilIURL *urls, char* type);
  
  virtual PRBool IsMulti();

  NS_IMETHOD FlushImgBuffer();

    // XXX Need to fix this to make sure return type is nsresult
  il_container *GetContainer() {return ilContainer;};
 // il_container *SetContainer(il_container *ic) {ilContainer=ic; return ic;};

private:
  il_container *ilContainer;
};

NetReaderImpl::NetReaderImpl(il_container *aContainer)
{
    NS_INIT_REFCNT();
    ilContainer = aContainer;
}

NetReaderImpl::~NetReaderImpl()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(NetReaderImpl, ilINetReader)

NS_IMETHODIMP
NetReaderImpl::WriteReady(PRUint32* max_read)
{
    if (ilContainer != NULL) {
        *max_read =IL_StreamWriteReady(ilContainer);      
    }
    return NS_OK;
}
  
NS_IMETHODIMP 
NetReaderImpl::FirstWrite(const unsigned char *str, int32 len, char* url)
{
    int ret = 0;
 
    if (ilContainer != NULL) {
		FREE_IF_NOT_NULL(ilContainer->fetch_url);
		if (url) {
			ilContainer->fetch_url = nsCRT::strdup(url);
		}
		else {
			ilContainer->fetch_url = NULL;
		}
        ret = IL_StreamFirstWrite(ilContainer, str, len);
        if(ret == 0)
            return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
NetReaderImpl::Write(const unsigned char *str, int32 len)
{
    int ret = 0;

    if (ilContainer != NULL) {
        ret=  IL_StreamWrite(ilContainer, str, len);
        if(ret >= 0)
            return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
NetReaderImpl::StreamAbort(int status)
{
    if (ilContainer != NULL) {
        IL_StreamAbort(ilContainer, status);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
NetReaderImpl::StreamComplete(PRBool is_multipart)
{
    if (ilContainer != NULL) {
        IL_StreamComplete(ilContainer, is_multipart);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
NetReaderImpl::NetRequestDone(ilIURL *urls, int status)
{
    if (ilContainer != NULL) {
        IL_NetRequestDone(ilContainer, urls, status);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}
  
PRBool 
NetReaderImpl::StreamCreated(ilIURL *urls, char* type)
{
    if (ilContainer != NULL) {
        return IL_StreamCreated(ilContainer, urls, type);
    }
    else {
        return PR_FALSE;
    }
}
  
PRBool 
NetReaderImpl::IsMulti()
{
    if (ilContainer != NULL) {
        return (PRBool)(ilContainer->multi > 0);
    }
    else {
        return PR_FALSE;
    }
}

NS_IMETHODIMP  
NetReaderImpl::FlushImgBuffer()
{
    if (ilContainer != NULL) {
        il_flush_image_data(ilContainer);
        return NS_OK;
    }
    else 
        return NS_ERROR_FAILURE;
    
}

ilINetReader *
IL_NewNetReader(il_container *ic)
{
    ilINetReader *reader = new NetReaderImpl(ic);

    if (reader != NULL) {
        NS_ADDREF(reader);
    }

    return reader;
}

il_container *
IL_GetNetReaderContainer(ilINetReader *reader)
{
    if (reader != NULL) {
        return ((NetReaderImpl *)reader)->GetContainer();
    }
    else {
        return NULL;
    }
}
