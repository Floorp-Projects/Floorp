/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0(the "NPL"); you may not use this file except in
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
 * Copyright(C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _FILESPECIMPL_H_
#define _FILESPECIMPL_H_

#include "nsIFileSpec.h" 
#include "nsFileSpec.h"

//========================================================================================
class NS_COM nsFileSpecImpl
//========================================================================================
	: public nsIFileSpec
{

 public: 

	NS_DECL_ISUPPORTS

  NS_DECL_NSIFILESPEC

	//----------------------
	// COM Cruft
	//----------------------

      static NS_METHOD Create(nsISupports* outer, const nsIID& aIID, void* *aIFileSpec);

	//----------------------
	// Implementation
	//----------------------

			nsFileSpecImpl();
			nsFileSpecImpl(const nsFileSpec& inSpec);
			virtual ~nsFileSpecImpl();
			static nsresult MakeInterface(const nsFileSpec& inSpec, nsIFileSpec** outSpec);

	//----------------------
	// Data
	//----------------------
	
		nsFileSpec							mFileSpec;
		nsIInputStream*						mInputStream;
		nsIOutputStream*					mOutputStream;

}; // class nsFileSpecImpl

//========================================================================================
class nsDirectoryIteratorImpl
//========================================================================================
	: public nsIDirectoryIterator
{

public:

	nsDirectoryIteratorImpl();
	virtual ~nsDirectoryIteratorImpl();

	NS_DECL_ISUPPORTS

	NS_IMETHOD Init(nsIFileSpec *parent, PRBool resolveSymlink);

	NS_IMETHOD exists(PRBool *_retval);

	NS_IMETHOD next();

	NS_IMETHOD GetCurrentSpec(nsIFileSpec * *aCurrentSpec);

	//----------------------
	// COM Cruft
	//----------------------

    static NS_METHOD Create(nsISupports* outer, const nsIID& aIID, void* *aIFileSpec);

protected:

	nsDirectoryIterator*					mDirectoryIterator;
}; // class nsDirectoryIteratorImpl

#endif // _FILESPECIMPL_H_
