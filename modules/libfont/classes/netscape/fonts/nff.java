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
   nff.java (Font.java)

   Interface definition of Font
   dp <dp@netscape.com>
*/


// ---------------------------------------------------------------------- 
// Font is an object that associates a FontName to a list of
// BaseFontHandle(s) that is returned by FontDisplayers that
// support this FontName.
//
// ---------------------------------------------------------------------- 

package netscape.fonts;

import netscape.jmc.*;

public interface nff {
	//
	// Returns the state of this FontObject
	// A Font object can be one of three states
	// NF_FONT_COMPLETE
	//		The font object is complete and usable in this state. All normal
	//		fonts (not webfonts) are in this state always. Once in this state,
	//		the font object will never switch its state back to any of the
	//		other states.
	// NF_FONT_INCOMPLETE
	//		A font object will assume this state if there is the font data
	//		is BEING DOWNLOADED. This state will happen only for webfonts.
	//		Users of this font object, will need to wait until the state
	//		switches to NF_FONT_COMPLETE to use the font object.
	// NF_FONT_ERROR
	//		If downloading a font was interrupted or was in error, then the
	//		font object takes this state. Users of the font are expected to
	//		release the font and take appropriate measures to react to
	//		the requested font not being available.
	//
	int GetState();

	//
	// List all sizes this font can support.
	// size '0' in the returned list will mean a scaled font
	//
	// NOTE: This allocates memory for the return list. Caller needs to
	//			free this memory using nffbu::free()
	//
	public double[] EnumerateSizes(nfrc rc);

	//
	// RenderableFont is the result of applying transformations
	// on this FontHandle. Currently supported transformations
	// are sizing, device-specific-transformation (screen/printer).
	// This translates to a call to the FontDisplayer.
	//
	public nfrf GetRenderableFont(nfrc rc, double pointsize);

	//
	// The Font Match Info that is returned will reflect the result
	// of the Lookup.
	public nffmi GetMatchInfo(nfrc rc, int pointsize);

	//
	// Query the font about the rc type it was created with
	//
	public int GetRcMajorType();
	public int GetRcMinorType();
}
