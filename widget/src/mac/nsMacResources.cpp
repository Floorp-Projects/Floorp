/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMacResources.h"
#include <Resources.h>


short nsMacResources::mRefNum				= kResFileNotOpened;
short nsMacResources::mSaveResFile	= 0;

#if !TARGET_CARBON
pascal OSErr __NSInitialize(const CFragInitBlock *theInitBlock);
pascal OSErr __initializeResources(const CFragInitBlock *theInitBlock);

pascal void __NSTerminate(void);
pascal void __terminateResources(void);

//----------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------
pascal OSErr __initializeResources(const CFragInitBlock *theInitBlock)
{
    OSErr err = __NSInitialize(theInitBlock);
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
    __NSTerminate();
}
#endif /*!TARGET_CARBON*/

//----------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------

nsresult nsMacResources::OpenLocalResourceFile()
{
#ifdef XP_MACOSX
  // XXX quick and dirty hack to make resources available so we don't crash.
	if (mRefNum == kResFileNotOpened) {
    FSSpec spec = { 0, 0, "\plibwidget.rsrc" };
    if (FindFolder(kUserDomain, kDomainLibraryFolderType, false, &spec.vRefNum, &spec.parID) == noErr)
      mRefNum = FSpOpenResFile(&spec, fsRdPerm);
  }
#endif
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

