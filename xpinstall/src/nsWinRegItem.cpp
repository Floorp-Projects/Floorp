/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsWinRegItem.h"
#include "nspr.h"

#ifdef WIN32
#include <windows.h> /* is this needed? */
#endif

/* Public Methods */

MOZ_DECL_CTOR_COUNTER(nsWinRegItem)

nsWinRegItem::nsWinRegItem(nsWinReg* regObj, PRInt32 root, PRInt32 action, const nsString& sub, const nsString& valname, const nsString& val, PRInt32 *aReturn)
: nsInstallObject(regObj->InstallObject())
{
    MOZ_COUNT_CTOR(nsWinRegItem);

    mReg     = regObj;
	mCommand = action;
	mRootkey = root;

  *aReturn = nsInstall::SUCCESS;

  /* I'm assuming we need to copy these */
	mSubkey  = new nsString(sub);
	mName    = new nsString(valname);
	mValue   = new nsString(val);

  if((mSubkey == nsnull) ||
     (mName   == nsnull) ||
     (mValue  == nsnull))
  {
    *aReturn = nsInstall::OUT_OF_MEMORY;
  }
}

nsWinRegItem::nsWinRegItem(nsWinReg* regObj, PRInt32 root, PRInt32 action, const nsString& sub, const nsString& valname, PRInt32 val, PRInt32 *aReturn)
: nsInstallObject(regObj->InstallObject())
{
    MOZ_COUNT_CTOR(nsWinRegItem);

	mReg     = regObj;
	mCommand = action;
	mRootkey = root;

  *aReturn = nsInstall::SUCCESS;

  /* I'm assuming we need to copy these */
	mSubkey  = new nsString(sub);
	mName    = new nsString(valname);
	mValue   = new PRInt32(val);

  if((mSubkey == nsnull) ||
     (mName   == nsnull) ||
     (mValue  == nsnull))
  {
    *aReturn = nsInstall::OUT_OF_MEMORY;
  }
}

nsWinRegItem::~nsWinRegItem()
{
  if (mSubkey)  delete mSubkey;
  if (mName)    delete mName;
  if (mValue)   delete mValue;
  MOZ_COUNT_DTOR(nsWinRegItem);
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
  
#define kCRK  "Create Registry Key: "
#define kDRK  "Delete Registry Key: "
#define kDRV  "Delete Registry Value: "
#define kSRVS "Store Registry Value String: "
#define kSRVN "Store Registry Value Number: "
#define kSRV  "Store Registry Value: "
#define kUNK  "Unknown "

char* nsWinRegItem::toString()
{
	nsString*  keyString     = nsnull;
	nsString*  result        = nsnull;
  char*      resultCString = nsnull;

	switch(mCommand)
	{
    case NS_WIN_REG_CREATE:
      keyString = keystr(mRootkey, mSubkey, nsnull);
      result    = new nsString;
      result->AssignWithConversion(kCRK);
      break;

    case NS_WIN_REG_DELETE:
      keyString = keystr(mRootkey, mSubkey, nsnull);
      result    = new nsString;
      result->AssignWithConversion(kDRK);
      break;

    case NS_WIN_REG_DELETE_VAL:
      keyString = keystr(mRootkey, mSubkey, mName);
      result    = new nsString;
      result->AssignWithConversion(kDRV);
     break;

    case NS_WIN_REG_SET_VAL_STRING:
      keyString = keystr(mRootkey, mSubkey, mName);
      result    = new nsString;
      result->AssignWithConversion(kSRVS);
      break;

    case NS_WIN_REG_SET_VAL_NUMBER:
      keyString = keystr(mRootkey, mSubkey, mName);
      result    = new nsString;
      result->AssignWithConversion(kSRVN);
      break;

    case NS_WIN_REG_SET_VAL:
      keyString = keystr(mRootkey, mSubkey, mName);
      result    = new nsString;
      result->AssignWithConversion(kSRV);
      break;

    default:
      keyString = keystr(mRootkey, mSubkey, mName);
      result    = new nsString;
      result->AssignWithConversion(kUNK);
      break;
	}
    
  if (result)
  {
      result->Append(*keyString);
      resultCString = new char[result->Length() + 1];
      if(resultCString != nsnull)
          result->ToCString(resultCString, result->Length() + 1);
  }
  
  if (keyString) delete keyString;
  if (result)    delete result;
  
  return resultCString;
}

PRInt32 nsWinRegItem::Prepare()
{
  PRInt32 aReturn = NS_OK;
  
  if (mReg == nsnull)
      return nsInstall::OUT_OF_MEMORY;

  switch (mCommand)
  {
    case NS_WIN_REG_CREATE:
        mReg->PrepareCreateKey(mRootkey, *mSubkey, &aReturn);
        break;
    
    case NS_WIN_REG_DELETE:
        mReg->PrepareDeleteKey(mRootkey, *mSubkey, &aReturn);
        break;
    
    case NS_WIN_REG_DELETE_VAL:
        mReg->PrepareDeleteValue(mRootkey, *mSubkey, *mName, &aReturn);
        break;
    
    case NS_WIN_REG_SET_VAL_STRING:
        mReg->PrepareSetValueString(mRootkey, *mSubkey, &aReturn);
        break;

    case NS_WIN_REG_SET_VAL_NUMBER:
        mReg->PrepareSetValueNumber(mRootkey, *mSubkey, &aReturn);
        break;
    
    case NS_WIN_REG_SET_VAL:
        mReg->PrepareSetValue(mRootkey, *mSubkey, &aReturn);
        break;

    default:
        break;
  }
	return aReturn;
}

void nsWinRegItem::Abort()
{
}

/* Private Methods */

nsString* nsWinRegItem::keystr(PRInt32 root, nsString* mSubkey, nsString* mName)
{
	nsString  rootstr;
	nsString* finalstr = nsnull;
  char*     istr     = nsnull;

	switch(root)
	{
	  case (int)(HKEY_CLASSES_ROOT) :
		  rootstr.AssignWithConversion("HKEY_CLASSES_ROOT\\");
		  break;

	  case (int)(HKEY_CURRENT_USER) :
		  rootstr.AssignWithConversion("HKEY_CURRENT_USER\\");
		  break;

	  case (int)(HKEY_LOCAL_MACHINE) :
		  rootstr.AssignWithConversion("HKEY_LOCAL_MACHINE\\");
		  break;

	  case (int)(HKEY_USERS) :
		  rootstr.AssignWithConversion("HKEY_USERS\\");
		  break;

    default:
      istr = itoa(root);
      if (istr)
      {
        rootstr.AssignWithConversion("#");
        rootstr.AppendWithConversion(istr);
        rootstr.AppendWithConversion("\\");
        
        PR_DELETE(istr);
      }
      break;
	}

  finalstr = new nsString(rootstr);
	if(finalstr != nsnull)
	{
    finalstr->Append(*mSubkey);
    finalstr->AppendWithConversion(" [");

    if(mName != nsnull)
      finalstr->Append(*mName);

    finalstr->AppendWithConversion("]");
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

