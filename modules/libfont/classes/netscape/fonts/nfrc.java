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





