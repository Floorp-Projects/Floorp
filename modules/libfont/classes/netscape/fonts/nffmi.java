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
