/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "if.h"
#include "ilINetReader.h"

static NS_DEFINE_IID(kINetReaderIID, IL_INETREADER_IID);

class NetReaderImpl : public ilINetReader {
public:

  NetReaderImpl(il_container *aContainer);
  virtual ~NetReaderImpl();

  NS_DECL_ISUPPORTS

  /*NS_IMETHOD*/virtual int WriteReady();
  
  virtual int FirstWrite(const unsigned char *str, int32 len);

  virtual int Write(const unsigned char *str, int32 len);

  virtual void StreamAbort(int status);

  virtual void StreamComplete(PRBool is_multipart);

  virtual void NetRequestDone(ilIURL *urls, int status);
  
  virtual PRBool StreamCreated(ilIURL *urls, int type);
  
  virtual PRBool IsMulti();

  il_container *GetContainer() {return ilContainer;}

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

NS_IMPL_ISUPPORTS(NetReaderImpl, kINetReaderIID)

int
NetReaderImpl::WriteReady()
{
    PRUint32 ret;

    if (ilContainer != NULL) {
        ret = IL_StreamWriteReady(ilContainer);
        if( ret != 0)
            return -1;
    }
    return 0;
}
  
int 
NetReaderImpl::FirstWrite(const unsigned char *str, int32 len)
{
    if (ilContainer != NULL) {
        return IL_StreamFirstWrite(ilContainer, str, len);
    }
    else {
        return 0;
    }
}

int 
NetReaderImpl::Write(const unsigned char *str, int32 len)
{
    if (ilContainer != NULL) {
        return IL_StreamWrite(ilContainer, str, len);
    }
    else {
        return 0;
    }
}

void 
NetReaderImpl::StreamAbort(int status)
{
    if (ilContainer != NULL) {
        IL_StreamAbort(ilContainer, status);
    }
}

void 
NetReaderImpl::StreamComplete(PRBool is_multipart)
{
    if (ilContainer != NULL) {
        IL_StreamComplete(ilContainer, is_multipart);
    }
}

void 
NetReaderImpl::NetRequestDone(ilIURL *urls, int status)
{
    if (ilContainer != NULL) {
        IL_NetRequestDone(ilContainer, urls, status);
    }
}
  
PRBool 
NetReaderImpl::StreamCreated(ilIURL *urls, int type)
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
