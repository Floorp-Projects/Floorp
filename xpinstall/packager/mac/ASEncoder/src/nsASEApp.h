/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Samir Gehani <sgehani@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 

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