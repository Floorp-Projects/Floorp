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
 
#include "nsInternetConfig.h"
#include "nsString.h"
#include "nsDebug.h"

#include <CodeFragments.h>   
#include <Processes.h>
ICInstance nsInternetConfig::sInstance = NULL;
long nsInternetConfig::sSeed = 0;
PRInt32  nsInternetConfig::sRefCount = 0;



static OSType GetAppCreatorCode()
{
  ProcessSerialNumber psn = { 0, kCurrentProcess } ;
  ProcessInfoRec      procInfo;
  
  procInfo.processInfoLength = sizeof(ProcessInfoRec);
  procInfo.processName = nsnull;
  procInfo.processAppSpec = nsnull;
  
  GetProcessInformation(&psn, &procInfo);
  return procInfo.processSignature;  
}



ICInstance nsInternetConfig::GetInstance()
{
	if ( !sInstance )
	{
		OSStatus err;
		if ((long)ICStart == kUnresolvedCFragSymbolAddress )
			return sInstance;                          
                                                                                 
                                                                                  
		OSType creator = GetAppCreatorCode();
		err = ::ICStart( &sInstance, creator  );
		if ( err != noErr )
		{
			::ICStop( sInstance );
		}
		else
		{
#if !TARGET_CARBON
			::ICFindConfigFile( sInstance, 0 , nil );
#endif
			::ICGetSeed( sInstance, &sSeed );
		}
	}
	return sInstance;
}

PRBool nsInternetConfig::HasSeedChanged()
{
	ICInstance instance = nsInternetConfig::GetInstance();
	if ( instance )
	{
		long newSeed = 0;
		::ICGetSeed( sInstance, &newSeed );
		if ( newSeed != sSeed )
		{
			sSeed = newSeed;
			return PR_TRUE;
		}
	}
	return PR_FALSE;
}

nsInternetConfig::nsInternetConfig()
{
	sRefCount++;
}

nsInternetConfig::~nsInternetConfig()
{
	sRefCount--;
	if ( sRefCount == 0 && sInstance)
	{
		::ICStop( sInstance );
		sInstance = NULL;
	}
}


nsresult nsInternetConfig::GetString( unsigned char* inKey, char** outString )
{
	nsresult result = NS_ERROR_FAILURE;
	ICInstance instance = nsInternetConfig::GetInstance();
	if ( instance )
	{	
		OSStatus err;
		char buffer[256];
		ICAttr junk;
		long size = 256;
		err = ::ICGetPref( instance, inKey, &junk, buffer, &size );
		if ( err == noErr )
		{
			if (size == 0) {
				*outString = nsnull;
				return NS_OK;
			}
				
			// Buffer is a Pascal string
			nsCString temp( &buffer[1], buffer[0] );
			*outString = temp.ToNewCString();
 			result = NS_OK;
		}
	}
	return result;
}