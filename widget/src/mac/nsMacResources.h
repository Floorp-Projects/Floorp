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

#ifndef nsMacResources_h__
#define nsMacResources_h__

#include "nsError.h"

class nsMacResources
{
public:
		static nsresult		OpenLocalResourceFile();
		static nsresult		CloseLocalResourceFile();

		// you shouldn't have to use these:
		static void 			SetLocalResourceFile(short aRefNum)	{mRefNum = aRefNum;}
		static short			GetLocalResourceFile()							{return mRefNum;}

private:
		static short		mRefNum;
		static short		mSaveResFile;
};

#endif //nsMacResources_h__

