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
   nfrf.java (RenderableFont.java)

   Interface definition of RenderableFont
   dp <dp@netscape.com>
*/


// ---------------------------------------------------------------------- 
// The final version of a Font. This Renderable Font can be used to
// DrawText, MeasureText on the device (RenderingContext) and pointsize
// that this was intended for.
// ---------------------------------------------------------------------- 

package netscape.fonts;

import netscape.jmc.*;

public interface nfrf {
	//
	// Returns the PointSize and RenderingContext this was created for.
	//
	public nffmi GetMatchInfo();
	public double GetPointSize();

	//
	// The return value of all these are in pixel sizes
	//
	public int GetMaxWidth();
	public int GetFontAscent();
	public int GetFontDescent();
	public int GetMaxLeftBearing();
	public int GetMaxRightBearing();

	public void SetTransformMatrix(float matrix[][]);
	public float [][] GetTransformMatrix();

	//
	// MeasureText and DrawText using this RenderableFont
	//

	public int MeasureText(nfrc rc, int charSpacing, byte bytes[],
						   int charLocs[]);
	public int MeasureBoundingBox(nfrc rc, int charSpacing, byte bytes[],
							BoundingBoxStar bb);
	
	public int DrawText(nfrc rc, int x, int y, int charSpacing, byte bytes[]);

	//
	// Prepare the rc for drawing. This can be used to optimize
	// multiple draw. If EndDraw() doesn't follow PrepareDraw(), the
	// result if undefined. After PrepareDraw(), depending on implementation
	// DrawText() might happen faster.
	//
	public int PrepareDraw(nfrc rc);
	public int EndDraw(nfrc rc);

}


