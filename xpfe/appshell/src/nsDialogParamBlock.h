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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef __nsDialogParamBlock_h
#define __nsDialogParamBlock_h

#include "nsIDialogParamBlock.h"
#include "nsString.h"

class nsDialogParamBlock: public nsIDialogParamBlock
{
 	enum {kNumInts = 8, kNumStrings =16 };
public: 	
		nsDialogParamBlock();
	virtual ~nsDialogParamBlock();
	 
    NS_DECL_NSIDIALOGPARAMBLOCK

  	// COM
	NS_DECL_ISUPPORTS	
private:
	nsresult InBounds( PRInt32 inIndex, PRInt32 inMax )
	{
		if ( inIndex >= 0 && inIndex< inMax )
			return NS_OK;
		else
			return NS_ERROR_ILLEGAL_VALUE;
	}
	
	PRInt32 mInt[ kNumInts ];
	PRInt32 mNumStrings;
	nsString* mString;  	
};

#endif

