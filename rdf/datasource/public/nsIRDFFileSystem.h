/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef	nsIRDFFileSystem_h__
#define	nsIRDFFileSystem_h__

#include "nscore.h"
#include "nsISupports.h"
#include "nsIRDFNode.h"

class nsVoidArray;


#define NS_IRDFFILESYSTEMDATAOURCE_IID \
{ 0x1222e6f0, 0xa5e3, 0x11d2, { 0x8b, 0x7c, 0x00, 0x80, 0x5f, 0x8a, 0x7d, 0xb5 } }

class nsIRDFFileSystemDataSource : public nsIRDFDataSource
{
public:
};



#define NS_IRDFFILESYSTEM_IID \
{ 0x204a1a00, 0xa5e4, 0x11d2, { 0x8b, 0x7c, 0x00, 0x80, 0x5f, 0x8a, 0x7d, 0xb5 } }

class nsIRDFFileSystem : public nsIRDFResource
{
public:
	NS_IMETHOD	GetFolderList(nsIRDFResource *source, nsVoidArray **array) const = 0;
	NS_IMETHOD	GetName(nsVoidArray **array) const = 0;
	NS_IMETHOD	GetURL(nsVoidArray **array) const = 0;
};



#endif // nsIRDFFileSystem_h__
