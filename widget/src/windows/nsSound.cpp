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


#include "nscore.h"
#include "nsISound.h"
#include "nsIAllocator.h"
#include "plstr.h"
#include "stdio.h"

#include <windows.h>

class SoundImpl : public nsISound
{
public:
    SoundImpl();
    virtual ~SoundImpl();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsISound interface

    NS_IMETHOD Beep();

};

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewSound(nsISound** aSound)
{
    NS_PRECONDITION(aSound != nsnull, "null ptr");
    if (! aSound)
        return NS_ERROR_NULL_POINTER;

    *aSound = new SoundImpl();
    if (! *aSound)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aSound);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////


SoundImpl::SoundImpl()
{
    NS_INIT_REFCNT();
}


SoundImpl::~SoundImpl()
{
}



NS_IMPL_ISUPPORTS(SoundImpl, nsCOMTypeInfo<nsISound>::GetIID());


NS_IMETHODIMP
SoundImpl::Beep()
{
	::MessageBeep(0);

    return NS_OK;
}
