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
   nfdlm.java (Class DynamicLoadableModule.java)

   This is an interface that the dynamic loadable module implements.
   dp <dp@netscape.com>
*/


package netscape.fonts;

import netscape.jmc.*;

public interface nfdlm {
	// Query a list of object IDs that this are creatable
	// JMCInterfaceIDStar ListInterface();

	// Returns nonzero if this nfdlm can create an object that supports
	// the passed in interfaceID. Else returns zero.
	int SupportsInterface(netscape.jmc.JMCInterfaceID interfaceID);

	// Create an instance of an object that supports the interface
	// interfaceID. Params is a list of creation specific parameters.
	Object CreateObject(netscape.jmc.JMCInterfaceID interfaceID,
			Object params[]);

	// OnUnload() will be called when the this dlm is to be unloaded.
	// If OnUnload() returns negative integer, then the caller will
	// not attempt to unload the dlm.
	int OnUnload();
}
