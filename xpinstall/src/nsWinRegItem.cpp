/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#include "nsWinRegItem.h"
#include "nspr.h"

#ifdef WIN32
#include <windows.h> /* is this needed? */
#endif

/* Public Methods */

nsWinRegItem::nsWinRegItem(nsWinReg* regObj, PRInt32 root, PRInt32 action, const nsString& sub, const nsString& valname, const nsString& val)
: nsInstallObject(regObj->InstallObject())
{
	mReg     = regObj;
	mCommand = action;
	mRootkey = root;

	/* I'm assuming we need to copy these */
	mSubkey  = new nsString(sub);
	mName    = new nsString(valname);
	mValue   = new nsString(val);
}

nsWinRegItem::nsWinRegItem(nsWinReg* regObj, PRInt32 root, PRInt32 action, const nsString& sub, const nsString& valname, PRInt32 val)
: nsInstallObject(regObj->InstallObject())
{
	mReg     = regObj;
	mCommand = action;
	mRootkey = root;

	/* I'm assuming we need to copy these */
	mSubkey  = new nsString(sub);
	mName    = new nsString(valname);
	mValue   = new PRInt32(val);
}

nsWinRegItem::~nsWinRegItem()
{
  if (mSubkey)  delete mSubkey;
  if (mName)    delete mName;
  if (mValue)   delete mValue;
}

PRInt32 nsWinRegItem::Complete()
{
  PRInt32 aReturn = NS_OK;
  
  if (mReg == nsnull)
      return nsInstall::OUT_OF_MEMORY;

  switch (mCommand)
  {
    case NS_WIN_REG_CREATE:
        mReg->FinalCreateKey(mRootkey, *mSubkey, *mName, &aReturn);
        break;
    
    case NS_WIN_REG_DELETE:
        mReg->FinalDeleteKey(mRootkey, *mSubkey, &aReturn);
        break;
    
    case NS_WIN_REG_DELETE_VAL:
        mReg->FinalDeleteValue(mRootkey, *mSubkey, *mName, &aReturn);
        break;
    
    case NS_WIN_REG_SET_VAL_STRING:
        mReg->FinalSetValueString(mRootkey, *mSubkey, *mName, *(nsString*)mValue, &aReturn);
        break;

    case NS_WIN_REG_SET_VAL_NUMBER:
        mReg->FinalSetValueNumber(mRootkey, *mSubkey, *mName, *(PRInt32*)mValue, &aReturn);
        break;
    
    case NS_WIN_REG_SET_VAL:
        mReg->FinalSetValue(mRootkey, *mSubkey, *mName, (nsWinRegValue*)mValue, &aReturn);
        break;
  }
	return aReturn;
}
  
#define kCRK "Create Registry Key "
#define kDRK "Delete Registry Key "
#define kDRV "Delete Registry Value "
#define kSRV "Store Registry Value "
#define kUNK "Unknown "

char* nsWinRegItem::toString()
{
	nsString* keyString = nsnull;
	nsString* result    = nsnull;
    char*     resultCString = nsnull;

	switch(mCommand)
	{
	    case NS_WIN_REG_CREATE:
		    keyString = keystr(mRootkey, mSubkey, nsnull);
            result    = new nsString(kCRK);

        case NS_WIN_REG_DELETE:
		    keyString = keystr(mRootkey, mSubkey, nsnull);
            result    = new nsString(kDRK);
	
        case NS_WIN_REG_DELETE_VAL:
		    keyString = keystr(mRootkey, mSubkey, mName);
            result    = new nsString(kDRV);

        case NS_WIN_REG_SET_VAL_STRING:
            keyString = keystr(mRootkey, mSubkey, mName);
            result    = new nsString(kSRV);

        case NS_WIN_REG_SET_VAL:
            keyString = keystr(mRootkey, mSubkey, mName);
            result    = new nsString(kSRV);

        default:
            keyString = keystr(mRootkey, mSubkey, mName);
            result    = new nsString(kUNK);
	}
    
    if (result)
    {
        result->Append(*keyString);
        resultCString = result->ToNewCString();
    }
    
    if (keyString) delete keyString;
    if (result)    delete result;
    
    return resultCString;
}

PRInt32 nsWinRegItem::Prepare()
{
	return nsnull;
}

void nsWinRegItem::Abort()
{
}

/* Private Methods */

nsString* nsWinRegItem::keystr(PRInt32 root, nsString* mSubkey, nsString* mName)
{
	nsString rootstr;
	nsString* finalstr = nsnull;
    char*     istr     = nsnull;

	switch(root)
	{
	    case (int)(HKEY_CLASSES_ROOT) :
		    rootstr = "\\HKEY_CLASSES_ROOT\\";
		    break;

	    case (int)(HKEY_CURRENT_USER) :
		    rootstr = "\\HKEY_CURRENT_USER\\";
		    break;

	    case (int)(HKEY_LOCAL_MACHINE) :
		    rootstr = "\\HKEY_LOCAL_MACHINE\\";
		    break;

	    case (int)(HKEY_USERS) :
		    rootstr = "\\HKEY_USERS\\";
		    break;

    	default:
            istr = itoa(root);
            if (istr)
            {
                rootstr = "\\#";
                rootstr.Append(istr);
                rootstr.Append("\\");
                
                PR_DELETE(istr);
            }
            break;
	}

    finalstr = new nsString(rootstr);
	if(mName != nsnull && finalstr != nsnull)
	{
        finalstr->Append(*mSubkey);
        finalstr->Append(" [");
        finalstr->Append(*mName);
        finalstr->Append("]");
	}

    return finalstr;
}


char* nsWinRegItem::itoa(PRInt32 n)
{
	char* s;
	int i, sign;
	if((sign = n) < 0)
		n = -n;
	i = 0;
	
	s = (char*)PR_CALLOC(sizeof(char));

	do
		{
		s = (char*)PR_REALLOC(s, (i+1)*sizeof(char));
		s[i++] = n%10 + '0';
		s[i] = '\0';
		} while ((n/=10) > 0);
		
	if(sign < 0)
	{
		s = (char*)PR_REALLOC(s, (i+1)*sizeof(char));
		s[i++] = '-';
	}
	s[i]  = '\0';
	reverseString(s);
	return s;
}

void nsWinRegItem::reverseString(char* s)
{
	int c, i, j;
	
	for(i=0, j=strlen(s)-1; i<j; i++, j--)
		{
		c = s[i];
		s[i] = s[j];
		s[j] = c;
		}
}

/* CanUninstall
* WinRegItem() does not install any files which can be uninstalled,
* hence this function returns false. 
*/
PRBool
nsWinRegItem:: CanUninstall()
{
    return PR_FALSE;
}

/* RegisterPackageNode
* WinRegItem() installs files which need to be registered,
* hence this function returns true.
*/
PRBool
nsWinRegItem:: RegisterPackageNode()
{
    return PR_TRUE;
}

