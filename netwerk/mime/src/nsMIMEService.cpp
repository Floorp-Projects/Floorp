/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Judson Valeski
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */

#include "nsMIMEService.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsMIMEInfoImpl.h"
#include "nsIURL.h"
#include "nsIFileChannel.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsMimeTypes.h"


#ifdef XP_MAC
#include "nsILocalFileMac.h"
#endif



// nsISupports methods
NS_IMPL_THREADSAFE_ISUPPORTS1(nsMIMEService, nsIMIMEService);

NS_METHOD
nsMIMEService::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult) {
    nsMIMEService* service = new nsMIMEService();
    if (!service) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(service);
    nsresult rv = service->Init();
    if (NS_FAILED(rv)) return rv;
    rv = service->QueryInterface(aIID, aResult);
    NS_RELEASE(service);
    return rv;
}

// nsMIMEService methods
nsMIMEService::nsMIMEService() {
    NS_INIT_REFCNT();
}

nsMIMEService::~nsMIMEService() {
}

nsresult
nsMIMEService::Init() {
    nsresult rv = NS_OK;
    rv = nsComponentManager::CreateInstance(NS_XMLMIMEDATASOURCE_CONTRACTID, nsnull, nsIMIMEDataSource::GetIID(), getter_AddRefs(mXML) );
  	
  	// Create native
		nsComponentManager::CreateInstance(NS_NATIVEMIMEDATASOURCE_CONTRACTID, nsnull, nsIMIMEDataSource::GetIID(), getter_AddRefs(mNative) );
   return rv;
}


// nsIMIMEService methods
NS_IMETHODIMP
nsMIMEService::GetFromExtension(const char *aFileExt, nsIMIMEInfo **_retval) {
    nsresult rv = NS_OK;
  	if( !mXML || NS_FAILED (rv = mXML->GetFromExtension( aFileExt, _retval ) ) )
  	{
  		if ( mNative )
  			rv = mNative->GetFromExtension( aFileExt, _retval );
  	}
  	return rv;
}



NS_IMETHODIMP
nsMIMEService::GetFromMIMEType(const char *aMIMEType, nsIMIMEInfo **_retval) {
		nsresult rv =NS_ERROR_FAILURE;
  	if( !mXML || NS_FAILED (rv = mXML->GetFromMIMEType( aMIMEType, _retval ) ) )
  	{
  		if ( mNative )
  			rv = mNative->GetFromMIMEType( aMIMEType, _retval );
  	}
  	return rv;
}



// Helper routines implemented in terms of the above

NS_IMETHODIMP
nsMIMEService::GetTypeFromExtension(const char *aFileExt, char **aContentType) {
    nsresult rv = NS_OK;;
    nsCOMPtr<nsIMIMEInfo> info;
    rv = GetFromExtension(aFileExt, getter_AddRefs(info));
    if (NS_FAILED(rv)) return rv;
   return info->GetMIMEType(aContentType);

}

NS_IMETHODIMP
nsMIMEService::GetTypeFromURI(nsIURI *aURI, char **aContentType) {
    nsresult rv = NS_ERROR_FAILURE;
    // first try to get a url out of the uri so we can skip post
    // filename stuff (i.e. query string)
    nsCOMPtr<nsIURL> url = do_QueryInterface(aURI, &rv);
    
    #ifdef XP_MAC
    	if ( NS_SUCCEEDED( rv ) )
    	{
    		nsXPIDLCString fileExt;
        url->GetFileExtension(getter_Copies(fileExt));
        
    		nsresult rv2;
    		nsCOMPtr<nsIFileURL> fileurl = do_QueryInterface( url, &rv2 );
    		if ( NS_SUCCEEDED ( rv2 ) )
    		{
    			nsCOMPtr <nsIFile> file;
    			rv2 = fileurl->GetFile( getter_AddRefs( file ) );
    			if ( NS_SUCCEEDED( rv2 ) )
    			{
    				rv2 = GetTypeFromFile( file, aContentType );
						if( NS_SUCCEEDED ( rv2 ) )
							return rv2;
					}			
    	}
    }
    #endif
    
    if (NS_SUCCEEDED(rv)) {
        nsXPIDLCString ext;
        rv = url->GetFileExtension(getter_Copies(ext));
        if (NS_FAILED(rv)) return rv;
        rv = GetTypeFromExtension(ext, aContentType);
        return rv;
    }

    nsXPIDLCString cStrSpec;
    // no url, let's give the raw spec a shot
    rv = aURI->GetSpec(getter_Copies(cStrSpec));
    if (NS_FAILED(rv)) return rv;

    nsAutoString specStr; specStr.AssignWithConversion(cStrSpec);

    // find the file extension (if any)
    nsAutoString extStr;
    PRInt32 extLoc = specStr.RFindChar('.');
    if (-1 != extLoc) {
        specStr.Right(extStr, specStr.Length() - extLoc - 1);
        char *ext = ToNewCString(extStr);
        if (!ext) return NS_ERROR_OUT_OF_MEMORY;
        rv = GetTypeFromExtension(ext, aContentType);
        nsMemory::Free(ext);
    }
    else
        return NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsMIMEService::GetTypeFromFile( nsIFile* aFile, char **aContentType )
{
		nsresult rv;
		nsCOMPtr<nsIMIMEInfo> info;
		
		// Get the Extension
		char* fileName;
		const char* ext = nsnull;
    rv = aFile->GetLeafName(&fileName);
    if (NS_FAILED(rv)) return rv;
    if (fileName != nsnull) {
      PRInt32 len = nsCRT::strlen(fileName); 
      for (PRInt32 i = len; i >= 0; i--) {
          if (fileName[i] == '.') {
              ext = &fileName[i + 1];
              break;
          }
      }
     }
     	nsCString fileExt( ext );       
			nsCRT::free(fileName);
		// Handle the mac case
#ifdef XP_MAC
		nsCOMPtr<nsILocalFileMac> macFile;
		
		macFile = do_QueryInterface( aFile, &rv );
		if ( NS_SUCCEEDED( rv ) )
		{
				PRUint32 type, creator;
				macFile->GetFileTypeAndCreator( (OSType*)&type,(OSType*) &creator );    			
				if( !mXML || NS_FAILED (rv = mXML->GetFromTypeCreator( type, creator, fileExt, getter_AddRefs(info) ) ) )
				{
					if ( mNative )
						rv = mNative->GetFromTypeCreator( type, creator, fileExt,  getter_AddRefs(info) );
				}
				
				if ( NS_SUCCEEDED( rv) )
				{
					rv = info->GetMIMEType(aContentType);
					if (NS_SUCCEEDED(rv)) {
						if (nsCRT::strcasecmp(*aContentType, TEXT_PLAIN))  // Let nsUnknownDecoder try to do better.
							return rv;
					}
				}
    }
#endif
// Windows, unix and mac when no type match occured.

      
		return GetTypeFromExtension( fileExt, aContentType );
}

