/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is nsANSIFileStreams.h, released March 23, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Patrick C. Beard <beard@netscape.com>
 */

#ifndef _nsANSIFileStreams_h_
#define _nsANSIFileStreams_h_

#include <stdio.h>

#include "nsIFileStreams.h"

class nsANSIInputStream : public nsIInputStream, public nsISeekableStream {
    FILE*       mFile;
    PRUint32    mSize;
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSISEEKABLESTREAM
    
    nsANSIInputStream(FILE* file);
    virtual ~nsANSIInputStream();
};

class nsANSIOutputStream : public nsIOutputStream, public nsISeekableStream {
    FILE* mFile;
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOUTPUTSTREAM
    NS_DECL_NSISEEKABLESTREAM
    
    nsANSIOutputStream(FILE* file);
    virtual ~nsANSIOutputStream();
};

#endif // _nsANSIFileStreams_h_
