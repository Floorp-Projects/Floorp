/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsNativeAppSupport.h"
 
#include <Gestalt.h>
#include <Dialogs.h>
#include <Resources.h>
 
 
static DialogPtr splashDialog = NULL;
static PicHandle picHandle;
#define rSplashDialog 512

void NS_ShowSplashScreen()
{
	
	splashDialog = ::GetNewDialog( rSplashDialog, nil, (WindowPtr)-1L );
	picHandle = GetPicture( rSplashDialog );
	SetWindowPic( splashDialog, picHandle );
	::ShowWindow( splashDialog );
	::SetPort( splashDialog );
	Rect rect = (**picHandle).picFrame;
	::DrawPicture( picHandle, &rect ); 
}

void NS_HideSplashScreen()
{
	if ( splashDialog )
	{
		ReleaseResource( (Handle)picHandle );
		SetWindowPic( splashDialog, NULL );
		DisposeWindow( splashDialog );
	}
}

bool NS_CanRun() 
{
	long response = 0;
	OSErr err = ::Gestalt (gestaltSystemVersion, &response);
	if ( err || response < 0x850)
	{
		::StopAlert (5000, NULL);
		return false;
	}
	return true;
}