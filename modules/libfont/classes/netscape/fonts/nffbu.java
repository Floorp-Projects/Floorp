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
   nffbu.java (FontBrokerUtility.java)

   Interface definition of FontBrokerUtility bag
   dp <dp@netscape.com>
*/


// ---------------------------------------------------------------------- 
// Notes:
// Used by			: Consumer viz. Layout, Java, IFC
// Implemented by	: FontBroker
//		The FontBrokerUtility bag has useful methods that can be used
//		by Consumers and Displayers. This is here because all other
//		components can share these.
// ---------------------------------------------------------------------- 

package netscape.fonts;

import netscape.jmc.*;

public interface nffbu {
	//
	// With a view that multiple users need not implement these shared
	// object, the FontBroker will implement these. Consumers and Displayers
	// can use these methods to create objects of interest if they agree to
	// call Object::Done() when they have no use
	// for the object. FontBroker will then decide to destroy the object at
	// its convinience. The FontBroker will use this Done() mechanism to
	// reclaim memory it allocated and for reference counting.
	//

	public nffmi CreateFontMatchInfo(ConstCString name, ConstCString charset,
		ConstCString encoding, int weight, int pitch, int style, int underline,
		int strikeOut, int resolutionX, int resolutionY);

	// majorType : NF_RC_DIRECT (1) for Direct drawing
	// obs[]: platform dependent list of parameters that will
	// be used to create the rendering context. It will be
	//			   obs[0]   obs[1]   obs[2]
	// X		: display, drawable, gc
	// win		: dc
	// mac		: graf_port
	//
	// majorType : NF_RC_BUFFER (2) is buffer (not yet supported)
	//
	// Valid minor types are
	// any negative number : Rc's are always different.
	// 0 (or) any positive number : Enable rc checking.
	//
	// Two rc's are set to be equivalent if their {major & minor} types match.
	// The same font can be used for equivalent rcs.

	public nfrc CreateRenderingContext(int majorType, int minorType,
									   Object obs[]);

	// This will create an observer whose update method will call into
	// the func that is passed in. This is a convinience for C and C++
	// consumers wanting to be notified when FontStreams are done.
	//
	// Java consumers wont need this. They can just create a class that
	// implements the nfdoer interface and implement the update mathod
	// there.
	//

	public nfdoer CreateFontObserver(nfFontObserverCallback func,
			Object call_data);

	// Memory allocator routines used by netscape-fonts
	//
	// netscape-fonts uses these allocation and free routines.
	// Any memory that could be freed by netscape-font and allocated elsewhere
	// should use these allocator routines. Also, other modules should
	// use these free routines when freeing memory allocated by
	// netscape-fonts
	public Object malloc(int size);
	public void free(Object mem);
	public Object realloc(Object mem, int size);

	//
	// Font Broker Preferences
	//

	// The following are font preference related queries

	public int IsWebfontsEnabled();
	public int EnableWebfonts();
	public int DisableWebfonts();

	// returns a list of FontDisplayers
	public ConstCString[] ListFontDisplayers();

	// returns if the FontDisplayer is enabled
	public int IsFontDisplayerEnabled(ConstCString displayer);

	// returns a List of Enabled FontDisplayers that support "mimetype"
	public ConstCString[] ListFontDisplayersForMimetype(ConstCString mimetype);

	// returns the FontDisplayer that is being used to handle the mimetype
	public ConstCString FontDisplayerForMimetype(ConstCString mimetype);


	// The following are used to change state

	public int EnableFontDisplayer(ConstCString displayer);
	public int DisableFontDisplayer(ConstCString displayer);
	public int EnableMimetype(ConstCString displayer, ConstCString mimetype);
	public int DisableMimetype(ConstCString displayer, ConstCString mimetype);

	//
	// Catalog specific
	//

	int LoadCatalog(ConstCString filename);
	int SaveCatalog(ConstCString filename);

	//
	// Webfonts handling routines. These are routines that use MWContext *
	//

	// Downloads the webfont url for this context and keep track of it
	int LoadWebfont(MWContextStar context, ConstCString url, int forceDownload);
	int ReleaseWebfonts(MWContextStar context);

	// Returns the number of successfully downloaded webfonts for this context
	int WebfontsNeedReload(MWContextStar context);

	// Call this to tell the broker that this page failed to load this font.
	// Broker might maintain a list of these to decide if WebfontsNeedReload()
	// On ReleaseWebfonts(), this list will be released.
	int LookupFailed(MWContextStar context, nfrc rc, nffmi fmi);

	//
	// Internationalization: Unicode conversion routines
	//

	// ToUnicode:
	//	Converts input string 'src[0..srcbytes-1]' in 'encoding' to unicode UCS2
	//	and writes it to dest.
	// Returns the number of unicode characters writen to dest
	int ToUnicode(ConstCString encoding, byte src[], short dest[]);

	// UnicodeLen:
	// Returns the number of shorts that will be needed for a unicode buffer to store
	// source buffer 'src[0..srcbytes-1]' in 'encoding'
	int UnicodeLen(ConstCString encoding, byte src[]);
}



