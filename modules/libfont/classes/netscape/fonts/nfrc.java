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
/* 
   nfrc.java (RenderingContext.java)

   Interface definition of RenderingContext
   dp <dp@netscape.com>
*/


// ---------------------------------------------------------------------- 
// Interface definition for the rendering context that will be needed
// by the MeasureText() and DrawText() interfaces to perform device
// and context specific tasks.
// ---------------------------------------------------------------------- 

package netscape.fonts;

import netscape.jmc.*;

public interface nfrc {
	//
	// Get the type of the rc
	// Two rc's are set to be equivalent if their {major & minor} types match.
	// The same font can be used for equivalent rcs.
	//
	// Valid major types are
	// NF_RC_DIRECT - An rc used for direct drawing on to the screen
	// NF_RC_BUFFER - An rc used for drawing to a buffer
	// NF_RC_INVALID - Dont use this rc anymore.
	//
	int GetMajorType();

	// Valid minor types are
	// any negative number : Rc's are always different.
	// 0 (or) any positive number : Enable rc checking.
	int GetMinorType();

	// returns positive if this rc is equivalent to the major and minor
	// types that are passed in. If not returns 0.
	int IsEquivalent(int majorType, int minorType);

	//
	// There will also be a platform specific part that will not be
	// opaque to the Displayers and consumers and opaque to broker.
	// The definition of this part is in nf.h
	//
	public PlatformRCData GetPlatformData();
	public int SetPlatformData(PlatformRCDataStar rcdata);
}





