/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
 
#include "nsIDialogParamBlock.h"
#include "nsString.h"
#include "nsXPComFactory.h"

class nsDialogParamBlock: public nsISupports
{
 	enum {kNumInts = 8, kNumStrings =12 };
public: 	
		nsDialogParamBlock();
	 	~nsDialogParamBlock();
	 
	NS_IMETHOD SetNumberStrings( PRInt32 inNumStrings ); 
 	NS_IMETHOD GetInt(PRInt32 inIndex, PRInt32 *_retval);
	NS_IMETHOD SetInt(PRInt32 inIndex, PRInt32 inInt);

  
  	NS_IMETHOD GetString(PRInt32 inIndex, PRUnichar **_retval);
  	NS_IMETHOD SetString(PRInt32 inIndex, const PRUnichar *inString);
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

nsDialogParamBlock::nsDialogParamBlock(): mNumStrings( 0 ), mString(NULL )
{
	NS_INIT_REFCNT();

	for( PRInt32 i =0; i< kNumInts; i++ )
		mInt[ i ] = 0;
}

nsDialogParamBlock::~nsDialogParamBlock()
{
	delete [] mString;
}

NS_IMETHODIMP nsDialogParamBlock::SetNumberStrings( PRInt32 inNumStrings )
{
	if ( mString != NULL )
	{
		return NS_ERROR_ALREADY_INITIALIZED;
	}
	 mString = new nsString[ inNumStrings ];
	 if ( !mString )
	 	return NS_ERROR_OUT_OF_MEMORY;
	 mNumStrings = inNumStrings;
         return NS_OK;
}


NS_IMETHODIMP nsDialogParamBlock::GetInt(PRInt32 inIndex, PRInt32 *_retval)
{
	nsresult rv = InBounds( inIndex, kNumInts );
	if ( rv == NS_OK )
		*_retval = mInt[ inIndex ];
	return rv;
}

NS_IMETHODIMP nsDialogParamBlock::SetInt(PRInt32 inIndex, PRInt32 inInt)
{
	nsresult rv = InBounds( inIndex, kNumInts );
	if ( rv == NS_OK )
		mInt[ inIndex ]= inInt;
	return rv;
}

  
NS_IMETHODIMP nsDialogParamBlock::GetString(PRInt32 inIndex, PRUnichar **_retval)
{
  	if ( mNumStrings == 0 )
  		SetNumberStrings( kNumStrings );
	nsresult rv = InBounds( inIndex, mNumStrings );
	if ( rv == NS_OK )
		*_retval = mString[ inIndex ].ToNewUnicode();
	return rv;
}

NS_IMETHODIMP nsDialogParamBlock::SetString(PRInt32 inIndex, const PRUnichar *inString)
{
	if ( mNumStrings == 0 )
		SetNumberStrings( kNumStrings );
	nsresult rv = InBounds( inIndex, mNumStrings );
	if ( rv == NS_OK )
		mString[ inIndex ]= inString;
	return rv;
}

static NS_DEFINE_IID(kIDialogParamBlock, nsIDialogParamBlock::GetIID() );
NS_IMPL_ADDREF(nsDialogParamBlock);
NS_IMPL_RELEASE(nsDialogParamBlock);
NS_IMPL_QUERY_INTERFACE(nsDialogParamBlock, kIDialogParamBlock);

// Entry point to create nsAppShellService factory instances...
NS_DEF_FACTORY(DialogParamBlock, nsDialogParamBlock)

nsresult NS_NewDialogParamBlockFactory(nsIFactory** aResult)
{
  nsresult rv = NS_OK;
  nsIFactory* inst;
  
  inst = new nsDialogParamBlockFactory;
  if (nsnull == inst)
  {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else
  {
    NS_ADDREF(inst);
  }
  *aResult = inst;
  return rv;
}
