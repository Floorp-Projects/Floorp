/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

//  XPCOM Class ID for the network data in-memory cache

#ifndef nsMEMCACHECID_h__
#define nsMEMCACHECID_h__

// {e4710560-7de2-11d3-90cb-0040056a906e}
#define NS_MEM_CACHE_FACTORY_CID                          \
    {                                                     \
        0xe4710560,                                       \
	    0x7de2,                                           \
        0x11d3,                                           \
	    {0x90, 0xcb, 0x00, 0x40, 0x05, 0x6a, 0x90, 0x6e}  \
    }

#endif // nsMEMCACHECID_h__
