/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
