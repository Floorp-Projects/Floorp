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
   nffmi.java (FontMatchInfo.java)

   Interface definition of FontMatchInfo
   dp <dp@netscape.com>
*/


// ---------------------------------------------------------------------- 
//
// The FontMatchInfo contains a list {attribute, value} pairs. Attributes
// can be added on the fly. There are however a set of predefined attributes.
// Attributes are ConstCStrings.
//
// The predefined attributes and the type of their values are
//		Attribute			Type of Value
//		---------------------------------
//		"nfFmiName"			ConstCString
//		"nfFmiCharset"		ConstCString
//		"nfFmiEncoding"		ConstCString
//		"nfFmiWeight"		int
//		"nfFmiPitch"		int
//		"nfFmiStyle"		int
//		"nfFmiUnderline"	int
//		"nfFmiOverstike"	int
//		"nfFmiPanose"		int[]
//		"nfFmiResolutionX"	int
//		"nfFmiResolutionY"	int
// ---------------------------------------------------------------------- 
// NOTE:
//	- FMI always takes a copy of the strings that are passed in either
//		for the attribute or value. On destruction, it frees all the strings.
//		For int and other values, the value is stored as such. It is the
//		responsibility of the creator of the fmi to free them.
//
//	- user defined attributes are not implemented yet. Issues:
//		1. We were planning on using a Fmi cache. Fmi<->Font mapping is
//			maintained in the broker. Checking FMI equality becomes critical.
// ---------------------------------------------------------------------- 

package netscape.fonts;

import netscape.jmc.*;

public interface nffmi {
	//
	// GetValue returns the value associated with the attribute
	// Returns NULL for invalid attributes
	//
	public Object GetValue(ConstCString attribute);

	//
	// The array of ConstCStrings returned is terminated with a NULL attribute.
	// This is the list of Attributes that are supported by this FMI.
	// This will include all the standard attributes, plus any more
	// that were specified at creation time.
	//
	// WARNING: Caller should NOT free (or) modify the list of
	// 			attributes returned
	//
	public ConstCString[] ListAttributes();

	//
	// User defined attributes.
	//
	// public int AddStringAttribute(ConstCString attribute, ConstCString value);
	// public int AddObjectAttribute(ConstCString attribute, Object value);
}
