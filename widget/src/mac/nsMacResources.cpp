/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsMacResources.h"
#include <Resources.h>


short nsMacResources::mRefNum				= kResFileNotOpened;
short nsMacResources::mSaveResFile	= 0;


pascal OSErr __initialize(const CFragInitBlock *theInitBlock);
pascal OSErr __initializeResources(const CFragInitBlock *theInitBlock);
pascal void __terminate(void);
pascal void __terminateResources(void);


//----------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------
pascal OSErr __initializeResources(const CFragInitBlock *theInitBlock)
{
    OSErr err = __initialize(theInitBlock);
    if (err)
    	return err;

	short saveResFile = ::CurResFile();

	short refNum = FSpOpenResFile(theInitBlock->fragLocator.u.onDisk.fileSpec, fsRdPerm);
	nsMacResources::SetLocalResourceFile(refNum);

	::UseResFile(saveResFile);

	return ::ResError();
}


//----------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------
pascal void __terminateResources(void)
{
	::CloseResFile(nsMacResources::GetLocalResourceFile());
    __terminate();
}


//----------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------

nsresult nsMacResources::OpenLocalResourceFile()
{
	if (mRefNum == kResFileNotOpened)
		return NS_ERROR_NOT_INITIALIZED;

	mSaveResFile = ::CurResFile();
	::UseResFile(mRefNum);

	return (::ResError() == noErr ? NS_OK : NS_ERROR_FAILURE);
}


//----------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------

nsresult nsMacResources::CloseLocalResourceFile()
{
	if (mRefNum == kResFileNotOpened)
		return NS_ERROR_NOT_INITIALIZED;

	::UseResFile(mSaveResFile);

	return (::ResError() == noErr ? NS_OK : NS_ERROR_FAILURE);
}

