/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

//    Functions for nsPersistentFileDescriptor that depend on streams, and so
//    cannot be made independent of base.

#include "nscore.h"
#include "nsError.h"

class nsPersistentFileDescriptor;
class nsInputStream;
class nsOutputStream;
class nsIInputStream;
class nsIOutputStream;
class nsFileURL;
class nsFileSpec;

NS_COM nsOutputStream& operator << (nsOutputStream& s, const nsFileURL& spec);
NS_COM nsresult ReadDescriptor(
	nsIInputStream* aStream, nsPersistentFileDescriptor&);
NS_COM nsresult WriteDescriptor(
	nsIOutputStream* aStream,
	const nsPersistentFileDescriptor&);
        // writes the data to a file
NS_COM nsInputStream& operator >> (
    nsInputStream&,
    nsPersistentFileDescriptor&);
        // reads the data from a stream (file or string)
NS_COM nsOutputStream& operator << (
    nsOutputStream&,
    const nsPersistentFileDescriptor&);
    	// writes the data to a  stream (file or string)
