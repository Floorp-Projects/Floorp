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
   nfdoer.java (FontObserver.java)

   Interface definition of DoneObserver
   dp <dp@netscape.com>
*/


// ---------------------------------------------------------------------- 
// Font Observer will be notified once streaming of a font is complete.
// ---------------------------------------------------------------------- 

package netscape.fonts;

import netscape.jmc.*;

public interface nfdoer {
	//
	// The font that was streamed was passed in as a parameter.
	// Before using the font, f.GetState() should be done to see
	// the font was downloaded ok.
	//
	public void Update(nff f);
}
