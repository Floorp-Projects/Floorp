/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
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
 
#include "nsMacMIMEDataSource.h"
#include "nsString.h"
#include "nsIComponentManager.h"
#include "nsIMIMEInfo.h"

#include <MacTypes.h>

// Yumm
static void ConvertCharStringToStr255( char* inString, Str255& outString  )
{
		if ( inString == NULL )
			return;
		PRInt32 len = nsCRT::strlen(inString);
		NS_ASSERTION( len<= 255 , " String is too big");
		if ( len> 255 )
		{
			len = 255;
			
		}
		memcpy(&outString[1], inString, len);
		outString[0] = len;
}

static nsresult MakeMIMEInfo( ICMapEntry &entry, nsIMIMEInfo*& info )
{
	// Create nsIMIMEInfo
	nsresult rv = nsComponentManager::CreateInstance(NS_MIMEINFO_CONTRACTID, nsnull, nsIMIMEInfo::GetIID(), &info );	  
	if ( NS_FAILED( rv ) )
		return rv;
	// Stuff in the data;
	info->SetMacCreator( entry.fileType );
	info->SetMacType( entry.fileCreator);
	
	nsCString description( (char*)&entry.entryName[1], entry.entryName[0]);
	nsString  unicodeDescription;
	unicodeDescription.AssignWithConversion ( description.get() );
	info->SetDescription( unicodeDescription.get() );
	

	nsCString mimetype((char*) &entry.MIMEType[1], entry.MIMEType[0] );
	info->SetMIMEType( mimetype.get() );
	
	// remove the .
	nsCString extension((char*) &entry.extension[2], entry.extension[0]-1 );
	info->AppendExtension( extension.get() );
	
	return NS_OK;
}

nsMacMIMEDataSource::nsMacMIMEDataSource()
{
	 NS_INIT_REFCNT();
}

nsMacMIMEDataSource::~nsMacMIMEDataSource()
{

}

NS_IMPL_ISUPPORTS1(nsMacMIMEDataSource,nsIMIMEDataSource);

NS_IMETHODIMP nsMacMIMEDataSource::GetFromMIMEType(const char *aType, nsIMIMEInfo **_retval)
{
	 return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMacMIMEDataSource::GetFromExtension(const char *aFileExt, nsIMIMEInfo **_retval)
{
	nsresult rv = NS_ERROR_FAILURE;
	ICInstance instance = mIC.GetInstance();
	if ( instance )
	{
		nsCString filename("foobar.");
		filename+=aFileExt;
		Str255 pFileName;
		ConvertCharStringToStr255( filename.get(), pFileName  );
		ICMapEntry entry;
		OSStatus err = ::ICMapFilename( instance, pFileName, &entry );
		if( err == noErr )
		{
			rv = MakeMIMEInfo( entry, *_retval );
		}
	}
	
	return rv;
}

NS_IMETHODIMP nsMacMIMEDataSource::GetFromTypeCreator(PRUint32 aType, PRUint32 aCreator, const char *aFileExt, nsIMIMEInfo **_retval)
{
	nsresult rv = NS_ERROR_FAILURE;
	ICInstance instance = mIC.GetInstance();
	if ( instance )
	{
		nsCString filename("foobar.");
		filename+=aFileExt;
		Str255 pFileName;
		ConvertCharStringToStr255( filename.get(), pFileName  );
		ICMapEntry entry;
		OSStatus err = ::ICMapTypeCreator( instance, aType, aCreator, pFileName, &entry );
		if( err == noErr )
		{
			rv = MakeMIMEInfo( entry, *_retval );
		}
	}
	return rv;
}


NS_IMETHODIMP nsMacMIMEDataSource::Add(nsIMIMEInfo *aInfo)
{
	 return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMacMIMEDataSource::Remove(const char *aType)
{
	 return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMacMIMEDataSource::GetEnumerator(nsISimpleEnumerator **_retval)
{
	 return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMacMIMEDataSource::Serialize(void)
{
	 return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMacMIMEDataSource::InitFromFile( nsIFile*)
{
	 return NS_ERROR_NOT_IMPLEMENTED;
}
