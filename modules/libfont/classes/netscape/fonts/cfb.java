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
   cfb.java (Class FontBroker.java)
   
   Implementation of FontBroker
   dp <dp@netscape.com>
*/


// ---------------------------------------------------------------------- 
// Notes:
//	- FontBroker implements 3 interfaces
//		a)	FontBrokerConsumer interface
//			Used by Consumers. This will include operations like
//			LookupFont, DefineFont.
//
//		b)	FontBrokerDisplayer interface
//			FontDisplayers will use this interface to register themselves
//			with the FontBroker.
//
//		c)	FontBrokerUtility interface
//			Methods common to both Displayers and consumers are implemented
//			here. These are generally useful methods and create common objects.
//
// ---------------------------------------------------------------------- 
//  

package netscape.fonts;
 
abstract
class cfb implements nffbc, nffbp, nffbu {
public cfb() {}
}
