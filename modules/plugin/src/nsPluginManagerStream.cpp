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

#include "nsPluginManagerStream.h"
#include "npglue.h"

////////////////////////////////////////////////////////////////////////////////
// Plugin Manager Stream Interface

nsPluginManagerStream::nsPluginManagerStream(NPP npp, NPStream* pstr)
    : npp(npp), pstream(pstr)
{
    NS_INIT_REFCNT();
    // XXX get rid of the npp instance variable if this is true
    PR_ASSERT(npp == ((np_stream*)pstr->ndata)->instance->npp);
}

nsPluginManagerStream::~nsPluginManagerStream(void)
{
}

static NS_DEFINE_IID(kIOutputStreamIID, NS_IOUTPUTSTREAM_IID);
NS_IMPL_ISUPPORTS(nsPluginManagerStream, kIOutputStreamIID);

NS_METHOD
nsPluginManagerStream::Close(void)
{
    NPError err = npn_destroystream(npp, pstream, nsPluginReason_Done);
    return (nsresult)err;
}

NS_METHOD
nsPluginManagerStream::Write(const char* aBuf, PRInt32 aOffset, PRInt32 aCount,
                             PRInt32 *resultingCount)
{
    PR_ASSERT(aOffset == 0);           // XXX need to handle the non-sequential write case
    PRInt32 rslt = npn_write(npp, pstream, aCount, (void*)aBuf);
    if (rslt == -1) 
        return NS_ERROR_FAILURE;       // XXX what should this be?
    *resultingCount = rslt;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
