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
   nffbc.java (FontBrokerConsumer.java)

   Interface definition of FontBrokerConsumer
   dp <dp@netscape.com>
*/


// ---------------------------------------------------------------------- 
// Notes:
// Used by			: Consumer viz. Layout, Java, IFC
// Implemented by	: FontBroker
//	- This is one of the interfaces implemented by the FontBroker
//		a)	FontBrokerConsumer interface
//			Used by Consumers. This will include operations like
//			LookupFont, DefineFont.
// ---------------------------------------------------------------------- 

package netscape.fonts;

import netscape.jmc.*;

public interface nffbc {
	// After use of the returned Font, the caller of these methods
	// should call the Release method.
	//
	// accessor is valid only for webfonts and it should be the
	// url of the page trying to access the webfont.
 	public nff LookupFont(nfrc rc, nffmi matchInfo, ConstCString accessor);

	// If faux is non-zero, then this method tries to
	// do fauxing and return a font that the Consumer can use
	// until the completion_callback gets notified.
	//
	// If the url defines multiple fonts, then the fmi will identify
	// the specific font to be created.
	//
	public nff CreateFontFromUrl(nfrc rc, ConstCString url_of_font,
		ConstCString url_of_page, int faux, nfdoer completion_callback,
		MWCntxStar context);

	// Creates a font from a fontfile. This is a synchronous call.
	public nff CreateFontFromFile(nfrc rc, ConstCString mimetype, ConstCString fontFile,
		ConstCString url_of_page);

	// Returns a list of fonts that match the input FontMatchInfo.
	// The list is an array of objects that implement the FontMatchInfo
	// interfaces. These correspond to the FontDisplayerCatalogue interface.
	//
	// NOTE: This allocates memory for the list of returned FMI array.
	//			Caller needs to free the list by calling nffbu::free()
	//			apart from releasing each of the fmi objects.
	public nffmi[] ListFonts(nfrc rc, nffmi fmi);

	// fmi should be a fully specified one, usually the result of a previous
	// call to nffbc::ListFonts()
	// rc should be the same as that was passed with the query to
	// nffbc::ListFonts()
	//
	// NOTE: This allocates memory for the return list. Caller needs to
	//			free this memory using nffbu::free()
	//
	public double[] ListSizes(nfrc rc, nffmi fmi);

	//
	// Given a RenderableFont, this method will return the Font object
	// the RenderableFont was created from.
	public nff GetBaseFont(nfrf rf);
}

