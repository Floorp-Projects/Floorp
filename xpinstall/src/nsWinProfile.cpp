/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#include "nsWinProfile.h"
#include "nsWinProfileItem.h"
#include "nspr.h"
#include <windows.h>

/* Public Methods */

MOZ_DECL_CTOR_COUNTER(nsWinProfile)

nsWinProfile::nsWinProfile( nsInstall* suObj, const nsString& folder, const nsString& file )
{
  MOZ_COUNT_CTOR(nsWinProfile);

  mFilename = new nsString(folder);
    
  if (mFilename)
  {
    if(mFilename->Last() != '\\')
    {
        mFilename->AppendWithConversion("\\");
    }
    mFilename->Append(file);

	mInstallObject = suObj;
  }
}

nsWinProfile::~nsWinProfile()
{
  if (mFilename)
      delete mFilename;

  MOZ_COUNT_DTOR(nsWinProfile);

}

PRInt32
nsWinProfile::GetString(nsString section, nsString key, nsString* aReturn)
{
  return NativeGetString(section, key, aReturn);
}

PRInt32
nsWinProfile::WriteString(nsString section, nsString key, nsString value, PRInt32* aReturn)
{
  *aReturn = NS_OK;
  
  nsWinProfileItem* wi = new nsWinProfileItem(this, section, key, value, aReturn);

  if(wi == nsnull)
  {
    *aReturn = nsInstall::OUT_OF_MEMORY;
    return NS_OK;
  }

  if(*aReturn != nsInstall::SUCCESS)
  {
    if(wi)
    {
      delete wi;
      return NS_OK;
    }
  }

  if (mInstallObject)
    mInstallObject->ScheduleForInstall(wi);
  
  return NS_OK;
}

nsString* nsWinProfile::GetFilename()
{
	return mFilename;
}

nsInstall* nsWinProfile::InstallObject()
{
	return mInstallObject;
}

PRInt32
nsWinProfile::FinalWriteString( nsString section, nsString key, nsString value )
{
	/* do we need another security check here? */
	return NativeWriteString(section, key, value);
}

/* Private Methods */

#define STRBUFLEN 255
  
PRInt32
nsWinProfile::NativeGetString(nsString section, nsString key, nsString* aReturn )
{
  int       numChars;
  char      valbuf[STRBUFLEN];
  char*     sectionCString;
  char*     keyCString;
  char*     filenameCString;

  /* make sure conversions worked */
  if(section.First() != '\0' && key.First() != '\0' && mFilename->First() != '\0')
  {
    sectionCString  = section.ToNewCString();
    keyCString      = key.ToNewCString();
    filenameCString = mFilename->ToNewCString();

    numChars        = GetPrivateProfileString(sectionCString, keyCString, "", valbuf, STRBUFLEN, filenameCString);

    aReturn->AssignWithConversion(valbuf);

    if (sectionCString)   Recycle(sectionCString);
    if (keyCString)       Recycle(keyCString);
    if (filenameCString)  Recycle(filenameCString);
  }

  return numChars;
}

PRInt32
nsWinProfile::NativeWriteString( nsString section, nsString key, nsString value )
{
  char* sectionCString;
  char* keyCString;
  char* valueCString;
  char* filenameCString;
  int   success = 0;

	/* make sure conversions worked */
  if(section.First() != '\0' && key.First() != '\0' && mFilename->First() != '\0')
  {
    sectionCString  = section.ToNewCString();
    keyCString      = key.ToNewCString();
    valueCString    = value.ToNewCString();
    filenameCString = mFilename->ToNewCString();

    success = WritePrivateProfileString( sectionCString, keyCString, valueCString, filenameCString );

    if (sectionCString)   Recycle(sectionCString);
    if (keyCString)       Recycle(keyCString);
    if (valueCString)     Recycle(valueCString);
    if (filenameCString)  Recycle(filenameCString);
  }

  return success;
}

