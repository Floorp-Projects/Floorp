/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/ 
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License 
 * for the specific language governing rights and limitations under the 
 * License. 
 * 
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are Copyright (C) 1999
 * Netscape Communications Corporation. All Rights Reserved.  
 * 
 * Contributors:
 *     Samir Gehani <sgehani@netscape.com>
 */
 

#ifndef _NS_ASEAPP_H_
#define _NS_ASEAPP_H_

#include <Events.h>
#include <Dialogs.h>
#include <Navigation.h>
#include <MacTypes.h>

class nsASEApp
{

public:
	nsASEApp();
	~nsASEApp();
	
	OSErr					Run();
	
	static void				SetCompletionStatus(Boolean aVal);
	static Boolean			GetCompletionStatus(void);
	static void				FatalError(short aErrID);
	static OSErr			GotRequiredParams(AppleEvent *appEvent);
	
private:
	void					InitManagers(void);
	void					InitAEHandlers(void);
	void					MakeMenus(void);
	
	WindowPtr				mWindow;
	AEEventHandlerUPP		mEncodeUPP;
	AEEventHandlerUPP		mDecodeUPP;
	AEEventHandlerUPP		mQuitUPP;
};

/*---------------------------------------------------------------*
 *   Callbacks
 *---------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif

pascal OSErr EncodeEvent(AppleEvent *appEvent, AppleEvent *reply, SInt32 handlerRefCon);
pascal OSErr DecodeEvent(AppleEvent *appEvent, AppleEvent *reply, SInt32 handlerRefCon);
pascal OSErr QuitEvent(AppleEvent *appEvent, AppleEvent *reply, SInt32 handlerRefCon);

#ifdef __cplusplus
}
#endif

extern Boolean gDone;


/*---------------------------------------------------------------*
 *   Errors
 *---------------------------------------------------------------*/
#define navLoadErr 128
#define aeInitErr  130

/*---------------------------------------------------------------*
 *   Resources
 *---------------------------------------------------------------*/
#define rMenuBar 				128
#define rMenuApple				128
#define rMenuItemAbout			1
#define rMenuFile 				129
#define rMenuItemASEncode		1
#define rMenuItemASDecode		2
#define rMenuItemASEncodeFolder 3
#define rMenuItemASDecodeFolder 4
#define rMenuItemQuit			6
#define rMenuEdit 				130

#define rAboutBox				129


/*---------------------------------------------------------------*
 *   Constants
 *---------------------------------------------------------------*/
#define kASEncoderEventClass	FOUR_CHAR_CODE('ASEn')
#define kAEEncode				FOUR_CHAR_CODE('enco')
#define kAEDecode				FOUR_CHAR_CODE('deco')

#endif /* _NS_ASEAPP_H_ */