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
   nffp.java (FontDisplayer.java)

   Interface definition of FontDisplayer
   dp <dp@netscape.com>
*/


// ---------------------------------------------------------------------- 
// The Object that serves Fonts.
// The FontDisplayer will only be used by the FontBroker. There would exist
// multiple FontDisplayers that server different types of fonts.
//
// Notes:
//	- The FontDisplayer need not do reference counting of Font objects.
//		The FontBroker does this for the FontDisplayer.
// ---------------------------------------------------------------------- 

package netscape.fonts;

import netscape.jmc.*;

public interface nffp {
	//
	// The Following methods reflect their equivalents in FontBroker
	//
	public Object LookupFont(nfrc rc, nffmi matchInfo, ConstCString accessor);


	// Creates a fonthandle from a fontfile. This is a synchronous call.
	public Object CreateFontFromFile(nfrc rc, ConstCString mimetype,
									 ConstCString fontFile, ConstCString urlOfPage);

	//
	// The stream created is passed the font data as it is downloaded.
	//
	public nfstrm CreateFontStreamHandler(nfrc rc, ConstCString urlOfPage);

	//
	// NOTE: This allocates memory for the return list. Caller needs to
	//			free this memory using nffbu::free()
	//
	public double[] EnumerateSizes(nfrc rc, Object fh);

	// A release mechanism for the object returned by LookupFont and DefineFont
	public int ReleaseFontHandle(Object fh);

	// The fmi that corrsponding to the font handle
	public nffmi GetMatchInfo(Object fh);

	//
	// RenderableFont is the result of applying transformations
	// on this BaseFontHandle. If BaseFontHandle is NULL, then
	// fontname is used instead. Currently supported transformations
	// are sizing
	//
	public nfrf GetRenderableFont(nfrc rc, Object fh, double pointsize);

	//
	// Catalogue Routines

	//
	// NOTE: This DOES NOT allocate memory for the return strings. Caller
	//			SHOULD NOT free it and should take a private copy for storing
	//			or modification of the returned strings.
	//
	public String Name();
	public String Description();

	//
	// The returned string is of the format
	// mime/type:suffix[,suffix]...:Description
	//   [;mime/type:suffix[,suffix]...:Description]...
	//
	// NOTE: This DOES NOT allocate memory for the return strings. Caller
	//			SHOULD NOT free it and should take a private copy for storing
	//			or modification of the returned strings.
	//
	public String EnumerateMimeTypes();

	
	//
	// Returns a list of fonts that match the input FontMatchInfo.
	// The list is an array of objects that implement the FontMatchInfo
	// interfaces. These correspond to the FontDisplayerCatalogue interface.
	//
	// NOTE: This allocates memory for the list of returned FMI array.
	//			Caller needs to free the list by calling nffbu::free()
	//			apart from releasing each of the fmi objects.
	//
	public nffmi[] ListFonts(nfrc rc, nffmi fmi);
	//
	// NOTE: This allocates memory for the return list. Caller needs to
	//			free this memory using nffbu::free()
	//
	public int[] ListSizes(nfrc rc, nffmi fmi);

	// If this returns a non-zero value, then the FontBroker will cache the
	// output of any Enumerate*() request and use that during the next
	// invocation instead of loading this dll/dso every time at startup
	// for getting this information.
	public int CacheFontInfo();
}
