/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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


