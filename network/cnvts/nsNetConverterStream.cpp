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

#include "nsNetConverterStream.h"
#include <stdio.h>
#include "net.h"
#include "nscore.h"


static NS_DEFINE_IID(kINetOStreamIID, NS_INETOSTREAM_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);


nsNetConverterStream :: nsNetConverterStream()
{
  NS_INIT_REFCNT();

  mNextStream = nsnull;
}


nsNetConverterStream :: ~nsNetConverterStream()
{
  mNextStream = nsnull;
}


NS_IMPL_ADDREF(nsNetConverterStream);
NS_IMPL_RELEASE(nsNetConverterStream);


nsresult nsNetConverterStream :: QueryInterface(const nsIID& aIID,
				              void** aInstancePtrResult)
{
  if (nsnull == aInstancePtrResult)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(kISupportsIID))
  {
    *aInstancePtrResult = (void *)((nsISupports *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(kINetOStreamIID))
  {
    *aInstancePtrResult = (void *)((nsINetOStream *)this);
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}


nsresult nsNetConverterStream :: Initialize(void *aStream)
{
	mNextStream = aStream;
	return NS_OK;
}


nsresult nsNetConverterStream :: Complete(void)
{
  NET_StreamClass *stream;

  stream = (NET_StreamClass *)mNextStream;
  if (stream != NULL)
  {
    stream->complete(stream);
    return NS_OK;
  }

  return NS_OK;
}


nsresult nsNetConverterStream :: Abort(PRInt32 status)
{
  NET_StreamClass *stream;

  stream = (NET_StreamClass *)mNextStream;
  if (stream != NULL)
  {
    stream->abort(stream, status);
    return NS_OK;
  }

  return NS_OK;
}


nsresult nsNetConverterStream :: WriteReady(PRUint32 *aReadyCount)
{
  NET_StreamClass *stream;

  stream = (NET_StreamClass *)mNextStream;
  if (stream != NULL)
  {
    unsigned int ready;

    ready = stream->is_write_ready(stream);
    *aReadyCount = ready;
    return NS_OK;
  }
  *aReadyCount = 0;
  return NS_OK;
}


nsresult nsNetConverterStream :: Write(const char* aBuf, PRUint32 aOffset, PRUint32 aCount, PRUint32 *aWriteCount)
{
  NET_StreamClass *stream;

  stream = (NET_StreamClass *)mNextStream;
  if (stream != NULL)
  {
    int ret;

    ret = stream->put_block(stream, (char *)(aBuf + aOffset), aCount);

    *aWriteCount = aCount;
    return NS_OK;
  }
  *aWriteCount = 0;
  return NS_OK;
}


nsresult nsNetConverterStream::Close(void)
{
  return NS_OK;
}

