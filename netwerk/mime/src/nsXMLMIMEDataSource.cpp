/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#include "nsXMLMIMEDataSource.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsMIMEInfoImpl.h"
#include "nsIURL.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsEnumeratorUtils.h"
#include "nsITransport.h"
#include "nsIFileTransportService.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsILocalFile.h"
#include "nsMimeTypes.h"

#include "nsIFileSpec.h"
#include "nsFileSpec.h"
#include "nsTextFormatter.h"
#include <string.h>
#include <stdio.h>
#include "prlong.h"

const char* kMIME="mime";
const char* kMIMEType="mimetype";
const char* kDescription="description";
const char* kExtensions="extensions";
const char* kMacCreator="maccreator";
const char* kMacType="mactype";


static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
// Hash table helper functions
static PRBool PR_CALLBACK DeleteEntry(nsHashKey *aKey, void *aData, void* closure) {
    nsMIMEInfoImpl *entry = (nsMIMEInfoImpl*)aData;
    NS_ASSERTION(entry, "mapping problem");
	NS_RELEASE(entry);
    return PR_TRUE;   
};

// nsISupports methods
NS_IMPL_THREADSAFE_ISUPPORTS1(nsXMLMIMEDataSource, nsIMIMEDataSource);

NS_METHOD
nsXMLMIMEDataSource::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult) {
    nsXMLMIMEDataSource* service = new nsXMLMIMEDataSource();
    if (!service) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(service);
    nsresult rv = service->Init();
    if (NS_FAILED(rv)) return rv;
    rv = service->QueryInterface(aIID, aResult);
    NS_RELEASE(service);
    return rv;
}

// nsXMLMIMEDataSource methods
nsXMLMIMEDataSource::nsXMLMIMEDataSource() {
    NS_INIT_REFCNT();
}

nsXMLMIMEDataSource::~nsXMLMIMEDataSource() {
    mInfoObjects->Reset(DeleteEntry, nsnull);
    delete mInfoObjects;
}

nsresult
nsXMLMIMEDataSource::Init() {
    nsresult rv = NS_OK;
    mInfoObjects = new nsHashtable();
    if (!mInfoObjects) return NS_ERROR_OUT_OF_MEMORY;

    rv = NS_NewISupportsArray(getter_AddRefs(mInfoArray));
    if (NS_FAILED(rv)) return rv;

    return  InitFromHack();
}

 /* This bad boy needs to retrieve a url, and parse the data coming back, and
  * add entries into the mInfoArray.
  */
nsresult
nsXMLMIMEDataSource::InitFromURI(nsIURI *aUri) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsXMLMIMEDataSource::AddMapping(const char* mimeType, 
                          const char* extension,
                          const char* description,
                          nsIURI* dataURI, PRUint32 type, PRUint32 creator)
{
    nsresult rv = NS_OK;
    // setup the new MIMEInfo object.
    nsMIMEInfoImpl* anInfo = new nsMIMEInfoImpl(mimeType);
    if (!anInfo) return NS_ERROR_OUT_OF_MEMORY;

    anInfo->mExtensions.AppendCString(nsCAutoString(extension));
    anInfo->mDescription.AssignWithConversion(description);
    anInfo->mURI = dataURI;

		anInfo->mMacType = type;
		anInfo->mMacCreator = creator;
    // The entry is mapped many-to-one and the MIME type is the root mapping.
    
    // First remove any existing mapping.
    rv = Remove(mimeType);
    if (NS_FAILED(rv)) return rv;

    // Next add the new root MIME mapping.
    nsCStringKey key(mimeType);
    nsMIMEInfoImpl* oldInfo = (nsMIMEInfoImpl*)mInfoObjects->Put(&key, anInfo);
    NS_ASSERTION(!oldInfo, "we just removed the entry, we shouldn't have one");
    NS_ADDREF(anInfo);

    rv = mInfoArray->AppendElement(anInfo); // update the root array.
    if (NS_FAILED(rv)) return rv;

    // Finally add an extension mapping.
    key = extension;
    oldInfo = (nsMIMEInfoImpl*)mInfoObjects->Put(&key, anInfo);
    NS_ASSERTION(!oldInfo, "file extension mappings should have been cleaned up in the RemoveMapping call");
    
    NS_ADDREF(anInfo);

    return NS_OK;
}

NS_IMETHODIMP
nsXMLMIMEDataSource::Add( nsIMIMEInfo* aMapper )
{
    if ( !aMapper )
        return NS_ERROR_NULL_POINTER;
			
    nsresult rv = NS_OK;
    
    nsXPIDLCString mimeType;
    rv = aMapper->GetMIMEType( getter_Copies( mimeType ) );
    if ( NS_FAILED( rv ) )
        return rv;
    // First remove any existing mapping.
    rv = Remove(mimeType);
    if (NS_FAILED(rv)) return rv;

    // Next add the new root MIME mapping.
    nsCStringKey key(mimeType);
    nsMIMEInfoImpl* oldInfo = (nsMIMEInfoImpl*)mInfoObjects->Put(&key, aMapper);
    NS_ASSERTION(!oldInfo, "we just removed the entry, we shouldn't have one");
    NS_ADDREF(aMapper);

    rv = mInfoArray->AppendElement(aMapper); // update the root array.
    if (NS_FAILED(rv)) return rv;

    // Finally add an extension mapping.
    char** extensions;
    PRUint32 count;
    rv = aMapper->GetFileExtensions(& count, &extensions );
    if ( NS_FAILED ( rv ) )
        return rv;
    for ( PRUint32 i = 0; i<count; i++ )
    {
        key = extensions[i];
        oldInfo = (nsMIMEInfoImpl*)mInfoObjects->Put(&key, aMapper);
        NS_ASSERTION(!oldInfo, "file extension mappings should have been cleaned up in the RemoveMapping call");
        NS_ADDREF(aMapper);
        nsMemory::Free( extensions[i] );
    }
    nsMemory::Free( extensions ); 
   
    return NS_OK;
}

// used to cleanup any file extension mappings when 
// a root MIME entry is removed.
static PRBool removeExts(nsCString& aElement, void *aData) {
    nsHashtable* infoObjects = (nsHashtable*)aData;
    NS_ASSERTION(infoObjects, "hash table botched up");

    nsCStringKey key(aElement);
    nsMIMEInfoImpl* info = (nsMIMEInfoImpl*)infoObjects->Remove(&key);
    NS_RELEASE(info);
    return PR_TRUE;
}

NS_IMETHODIMP
nsXMLMIMEDataSource::Remove(const char* aMIMEType) {
    nsresult rv = NS_OK;
    nsCStringKey key(aMIMEType);

    // First remove the root MIME mapping.
    nsMIMEInfoImpl* info = (nsMIMEInfoImpl*)mInfoObjects->Remove(&key);
    if (!info) return NS_OK;

    rv = mInfoArray->RemoveElement(info); // update the root array.
    if (NS_FAILED(rv)) return rv;

    // Next remove any file association mappings.
    rv = info->mExtensions.EnumerateForwards(removeExts, mInfoObjects);
    NS_RELEASE(info);
    if (NS_FAILED(rv)) return rv;

		// mMacCache is potentially invalid
		mMacCache.Reset();
		
    return NS_OK;
}

nsresult
nsXMLMIMEDataSource::AppendExtension(const char* mimeType, const char* extension) {
    nsCStringKey key(mimeType);

    nsMIMEInfoImpl* info = (nsMIMEInfoImpl*)mInfoObjects->Get(&key);
    if (!info) return NS_ERROR_FAILURE;

    info->mExtensions.AppendCString(nsCAutoString(extension));

    // Add another file extension mapping.
    key = extension;
    nsMIMEInfoImpl* oldInfo = (nsMIMEInfoImpl*)mInfoObjects->Put(&key, info);
    NS_IF_RELEASE(oldInfo); // overwrite any existing mapping.
    NS_ADDREF(info);

    return NS_OK;
}

nsresult
nsXMLMIMEDataSource::RemoveExtension(const char* aExtension) {
    nsresult rv = NS_OK;
    nsCStringKey key(aExtension);

    // First remove the extension mapping.
    nsMIMEInfoImpl* info = (nsMIMEInfoImpl*)mInfoObjects->Remove(&key);
    if (!info) return NS_ERROR_FAILURE;
    
    // Next remove the root MIME mapping from the array and hash
    // IFF this was the only file extension mapping left.
    PRBool removed = info->mExtensions.RemoveCString(nsCAutoString(aExtension));
    NS_ASSERTION(removed, "mapping problem");

    if (info->GetExtCount() == 0) {
        // it's empty, remove the root mapping from hash and array.
        nsXPIDLCString mimeType;
        rv = info->GetMIMEType(getter_Copies(mimeType));
        if (NS_FAILED(rv)) return rv;

        key = (const char*)mimeType;
        nsMIMEInfoImpl* rootEntry = (nsMIMEInfoImpl*)mInfoObjects->Remove(&key);
        NS_ASSERTION(rootEntry, "mapping miss-hap");

        rv = mInfoArray->RemoveElement(rootEntry); // update the root array
        if (NS_FAILED(rv)) return rv;

        NS_RELEASE(rootEntry);
    }
    
    NS_RELEASE(info);

    return NS_OK;
}

NS_IMETHODIMP
nsXMLMIMEDataSource::GetEnumerator(nsISimpleEnumerator* *aEnumerator) {
		return NS_NewArrayEnumerator( aEnumerator, mInfoArray);
 
}

NS_IMETHODIMP
nsXMLMIMEDataSource::Serialize() {

  nsresult rv = NS_OK;
  nsCOMPtr<nsITransport> transport;

  nsCOMPtr<nsIFileTransportService> fts = 
           do_GetService(kFileTransportServiceCID, &rv) ;
  if(NS_FAILED(rv)) return rv ;
 
  rv = fts->CreateTransport(mFile, PR_WRONLY|PR_CREATE_FILE, PR_IRWXU, getter_AddRefs(transport)) ;
  if(NS_FAILED(rv))
    return rv ;
  
  // we don't need to worry about notification callbacks
  nsCOMPtr<nsIOutputStream> stream;
  rv = transport->OpenOutputStream(0, -1, 0, getter_AddRefs( stream ) ) ;
	if(NS_FAILED(rv))
    return rv ;
	nsCOMPtr<nsISimpleEnumerator> enumerator;	
	rv = GetEnumerator( getter_AddRefs( enumerator ) );
  if ( NS_FAILED( rv ) )
  	return rv;
  nsCString buffer;
  nsXPIDLCString cdata;
 
  
  buffer="<?xml version=\"1.0\"?>\r";
  PRUint32 bytesWritten;
  PRBool more;
 
	rv = stream->Write( buffer.get() , buffer.Length(), &bytesWritten );

  while ( NS_SUCCEEDED( enumerator->HasMoreElements(& more ) )&& more ) 
	{
		nsCOMPtr<nsIMIMEInfo> info;
	  rv = enumerator->GetNext( getter_AddRefs( info ) );
	  if ( NS_FAILED ( rv ) )
			return rv;
			
		buffer="<mimetype ";
		
		PRUnichar* unidata;
		rv = info->GetDescription( &unidata  );
		if ( NS_FAILED ( rv ) )
			return rv;
		buffer+= kDescription;
		buffer+="=\"";
		nsString temp( unidata );
		nsMemory::Free( unidata );
		char* utf8 = ToNewUTF8String(temp);
		buffer+=utf8;
		nsMemory::Free( utf8 );
		buffer+="\" ";
		
		rv = info->GetMIMEType( getter_Copies( cdata ) );
		if ( NS_FAILED ( rv ) )
			return rv;
		buffer+=kMIMEType;
		buffer+="=\"";
		buffer+=cdata;
		buffer+="\" ";
		
		char** extensions;
		PRUint32 count;
		rv = info->GetFileExtensions(& count, &extensions );
		if ( NS_FAILED ( rv ) )
			return rv;
		buffer+=kExtensions;
		buffer+="=\"";
		for ( PRUint32 i = 0; i<(count-1); i++ )
		{
			buffer+=extensions[i];
			buffer+=",";
			nsMemory::Free( extensions[i] );
		}
		buffer+=extensions[count-1];
		buffer+="\" ";
		nsMemory::Free( extensions[count-1] );
		nsMemory::Free( extensions );
			
		PRUint32 macData;
		char macBuffer[8];
		rv = info->GetMacCreator( &macData );
		if ( NS_FAILED ( rv ) )
			return rv;
		buffer+=kMacCreator;
		buffer+="=\"";
		sprintf( macBuffer,"%x" ,macData );
		buffer+=macBuffer;
		buffer+="\" ";
		
		rv = info->GetMacType( &macData );
		if ( NS_FAILED ( rv ) )
			return rv;
		buffer+=kMacType;
		buffer+="=\"";
		sprintf( macBuffer,"%x" ,macData );
		buffer+=macBuffer;
		buffer+="\" ";
		
		
		buffer+="/>\r";
	
		rv = stream->Write( buffer.get() , buffer.Length(), &bytesWritten );
	
  	if ( NS_FAILED( rv ) )
  		return rv;
  }
  rv = stream->Close();

  return rv;
}

nsresult
nsXMLMIMEDataSource::InitFromHack() {
    nsresult rv;

    rv = AddMapping(TEXT_PLAIN, "txt", "Text File", nsnull, 'TEXT','ttxt');
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(TEXT_PLAIN, "text");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(APPLICATION_OCTET_STREAM, "exe", "Binary Executable", nsnull);
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(APPLICATION_OCTET_STREAM, "bin");
    if (NS_FAILED(rv)) return rv;
#if defined(VMS)
    rv = AppendExtension(APPLICATION_OCTET_STREAM, "sav");
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(APPLICATION_OCTET_STREAM, "bck");
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(APPLICATION_OCTET_STREAM, "pcsi");
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(APPLICATION_OCTET_STREAM, "dcx_axpexe");
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(APPLICATION_OCTET_STREAM, "dcx_vaxexe");
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(APPLICATION_OCTET_STREAM, "sfx_axpexe");
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(APPLICATION_OCTET_STREAM, "sfx_vaxexe");
    if (NS_FAILED(rv)) return rv;
#endif

    rv = AddMapping(TEXT_HTML, "htm", "HyperText Markup Language", nsnull);
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(TEXT_HTML, "html");
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(TEXT_HTML, "shtml");
    if (NS_FAILED(rv)) return rv;
		
		rv = AppendExtension(TEXT_HTML, "ehtml");
		if (NS_FAILED(rv)) return rv;

    rv = AddMapping(TEXT_RDF, "rdf", "Resource Description Framework", nsnull);
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(TEXT_XUL, "xul", "XML-Based User Interface Language", nsnull, 'TEXT','ttxt' );
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(TEXT_XML, "xml", "Extensible Markup Language", nsnull, 'TEXT','ttxt');
    if (NS_FAILED(rv)) return rv;

    rv = AppendExtension(TEXT_XML, "xhtml"); // Extensible HyperText Markup Language
    if (NS_FAILED(rv)) return rv;
    
    rv = AppendExtension(TEXT_XML, "xht");   // Extensible HyperText Markup Language
    if (NS_FAILED(rv)) return rv;

    rv = AppendExtension(TEXT_XML, "xsl");
    if (NS_FAILED(rv)) return rv;

#ifdef MOZ_SVG
    rv = AddMapping("image/svg+xml", "svg", "Scalable Vector Graphics", nsnull, 'svg ', 'ttxt');
    if (NS_FAILED(rv)) return rv;
#endif

    rv = AddMapping(TEXT_CSS, "css", "Style Sheet", nsnull, 'TEXT','ttxt');
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(APPLICATION_JAVASCRIPT, "js", "Javascript Source File", nsnull, 'TEXT','ttxt');
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(MESSAGE_RFC822, "eml", "RFC-822 data", nsnull);
    if (NS_FAILED(rv)) return rv;
    
    rv = AddMapping(APPLICATION_GZIP2, "gz", "gzip", nsnull);
    if (NS_FAILED(rv)) return rv;


    rv = AddMapping(IMAGE_GIF, "gif", "GIF Image", nsnull, 'GIFf','GCon' );
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(IMAGE_JPG, "jpeg", "JPEG Image", nsnull, 'JPEG', 'GCon' );
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(IMAGE_JPG, "jpg");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(IMAGE_BMP, "bmp", "BMP Image", nsnull);
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(IMAGE_BMP, "bmp");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(IMAGE_ICO, "ico", "ICO Image", nsnull);
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(IMAGE_ICO, "ico");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(IMAGE_PNG, "png", "PNG Image", nsnull);
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(IMAGE_ART, "art", "ART Image", nsnull);
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(IMAGE_TIFF, "tiff", "TIFF Image", nsnull);
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(IMAGE_TIFF, "tif");
    if (NS_FAILED(rv)) return rv;

    rv = AddMapping(APPLICATION_POSTSCRIPT, "ps", "Postscript File", nsnull);
    if (NS_FAILED(rv)) return rv;
    rv = AppendExtension(APPLICATION_POSTSCRIPT, "eps");
    if (NS_FAILED(rv)) return rv;

    rv = AppendExtension(APPLICATION_POSTSCRIPT, "ai");
    if (NS_FAILED(rv)) return rv;
    
    rv = AddMapping( "application/x-arj", "arj", "ARJ file", nsnull);
    if (NS_FAILED(rv)) return rv;
    
    rv = AddMapping(APPLICATION_XPINSTALL, "xpi", "XPInstall Install", nsnull, 'xpi*','MOSS');
    if (NS_FAILED(rv)) return rv;
    
    return NS_OK;
}


// nsIMIMEService methods
NS_IMETHODIMP
nsXMLMIMEDataSource::GetFromExtension(const char *aFileExt, nsIMIMEInfo **_retval) {
    // for now we're assuming file extensions are case insensitive.

    nsCAutoString fileExt(aFileExt);
    fileExt.ToLowerCase();

    nsCStringKey key(fileExt.get());

    nsMIMEInfoImpl *entry = (nsMIMEInfoImpl*)mInfoObjects->Get(&key);
    if (!entry) return NS_ERROR_FAILURE;
    NS_ADDREF(entry);
    *_retval = NS_STATIC_CAST(nsIMIMEInfo*, entry);
    return NS_OK;
}



NS_IMETHODIMP
nsXMLMIMEDataSource::GetFromMIMEType(const char *aMIMEType, nsIMIMEInfo **_retval) {
    nsCAutoString MIMEType(aMIMEType);
    MIMEType.ToLowerCase();

    nsCStringKey key(MIMEType.get());

    nsMIMEInfoImpl *entry = (nsMIMEInfoImpl*)mInfoObjects->Get(&key);
    if (!entry) return NS_ERROR_FAILURE;
    NS_ADDREF(entry);
    *_retval = NS_STATIC_CAST(nsIMIMEInfo*, entry);
    return NS_OK;
}

NS_IMETHODIMP
nsXMLMIMEDataSource::GetFromTypeCreator(PRUint32 aType, PRUint32 aCreator, const char* aExt,  nsIMIMEInfo **_retval)
{	    
      // STRING USE WARNING: this use should be examined
    PRUint32 buf[2];
    buf[0] = aType;
    buf[1] = aCreator;
    nsCAutoString keyString((char*)buf,8);
    keyString += aExt;
    nsCStringKey key(keyString);
    // Check if in cache for real quick look up of common ( html,js, xul, ...) types
		nsIMIMEInfo *entry = (nsIMIMEInfo*)mMacCache.Get(&key);
    if (entry)
    {
    	NS_ADDREF(entry);
   	  *_retval = entry;
   		return NS_OK;
	 }
	 // iterate through looking for a match
	 // must match type, bonus points for ext, and app
	 // use the same scoring as IC
	 PRInt32 score = 0;
	 nsCOMPtr<nsISimpleEnumerator> enumerator;
	 nsresult rv = GetEnumerator( getter_AddRefs( enumerator ) );
	 if ( NS_FAILED( rv ) )
	 		return rv;
	 nsCOMPtr< nsIMIMEInfo> info;
	 nsCOMPtr<nsIMIMEInfo> bestMatch;
	 PRBool hasMore;
	 nsCString extString ( aExt );
	 while( NS_SUCCEEDED(enumerator->HasMoreElements( & hasMore ) ) && hasMore )
	 {
	 		enumerator->GetNext( getter_AddRefs( info ) );
	 		PRUint32 type, creator;
	 		info->GetMacType( &type );
	 		info->GetMacCreator( &creator );
	 		
	 		PRInt32 scoreOfElement  = 0;
	 		if ( type == aType )
	 		{
	 			scoreOfElement=2;
	 		}
	 		
	 		
	 		if ( creator == aCreator )
	 				scoreOfElement++;
	 			
	 			PRBool tempBool = PR_FALSE;
	 			info->ExtensionExists( aExt, &tempBool );
	 			if ( tempBool )
	 				scoreOfElement+= 4; 		
	 		
	 			if ( scoreOfElement > score )
	 			{
	 				score = scoreOfElement;
	 				bestMatch = info;
	 			}
	 }
	 
	 
	 if( score != 0 )
	 {
	 		*_retval = bestMatch;
	 		NS_ADDREF( 	*_retval );
	 		
	 		// Add to cache
	 		mMacCache.Put( &key, bestMatch.get() );
	 		return NS_OK;
	 }
	 
	 return NS_ERROR_FAILURE;
}

// Parser
// unicode "%s" format string
static const PRUnichar unicodeFormatter[] = {
    (PRUnichar)'%',
    (PRUnichar)'s',
    (PRUnichar)0,
};


static nsresult convertUTF8ToUnicode(const char *utf8String, PRUnichar ** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    // convert to PRUnichar using nsTextFormatter
    *aResult = nsTextFormatter::smprintf(unicodeFormatter, utf8String);
    if (! *aResult) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

static nsresult AddAttribute( nsIMIMEInfo* inElement, nsCString& inAttribute, nsCString& inValue )
{
    /*
      Note: maybe the constants |kMIMEType|, |kDescription|, |kExtensions|, etc., would be
        better defined using |NS_LITERAL_CSTRING| to avoid the complicated constructions
        below, and constant re-calculation of length.
     */
	nsresult rv = NS_OK;
	if ( inAttribute == nsDependentCString(kMIMEType) )
	{
		rv = inElement->SetMIMEType( inValue.get() );
	}
	else if ( inAttribute == nsDependentCString(kDescription)  )
	{
		PRUnichar* unicode; 
		convertUTF8ToUnicode( inValue.get(), &unicode );
		rv =inElement->SetDescription( unicode );
		nsTextFormatter::smprintf_free(unicode);
	}
	else if ( inAttribute == nsDependentCString(kExtensions)   )
	{
		rv = inElement->SetFileExtensions( inValue.get() );
	}
	else if ( inAttribute == nsDependentCString(kMacType) )
	{
		PRUint32 value;
		sscanf ( inValue.get(), "%x", &value);
		rv = inElement->SetMacType( value );
	}
	else if ( inAttribute == nsDependentCString(kMacCreator) )
	{
		PRUint32 value;
		sscanf ( inValue.get(), "%x", &value);
		rv = inElement->SetMacCreator( value );
	}
	
	return rv;
}


class StDeallocator 
{ 
public: 
 StDeallocator( void* memory): mMemory( memory ){}; 
 ~StDeallocator() 
 { 
  if (mMemory) 
        nsMemory::Free(mMemory); 
  } 
private: 
 void* mMemory; 
}; 

nsresult
nsXMLMIMEDataSource::InitFromFile( nsIFile*  aFile  )
{
 nsresult rv;
 nsCOMPtr<nsITransport> transport;

 nsCOMPtr<nsIFileTransportService> fts = 
          do_GetService(kFileTransportServiceCID, &rv) ;
 if(NS_FAILED(rv)) return rv ;
 // Made second parameter 0 since I really don't know what it is used for
 rv = fts->CreateTransport(aFile, PR_RDONLY, PR_IRWXU, getter_AddRefs(transport)) ;
 if(NS_FAILED(rv))
   return rv ;
  
  // we don't need to worry about notification callbacks
 nsCOMPtr<nsIInputStream> stream;
 rv = transport->OpenInputStream(0, -1, 0, getter_AddRefs( stream ) ) ;
 if(NS_FAILED(rv))  return rv ;
    
 PRUint32 streamLength;
 PRInt64 fileLength;
 rv = aFile->GetFileSize( &fileLength );
 if( NS_FAILED(rv)) return rv ;
 
 LL_L2I( streamLength,fileLength  );
 
 char* buffer = new char[streamLength ];
 if ( !buffer )
 		return NS_ERROR_OUT_OF_MEMORY;

 StDeallocator dealloc( buffer );
 PRUint32 amtRead;
 
 rv = stream->Read( buffer, streamLength , &amtRead );		
 if ( NS_FAILED( rv ) ) return rv;	
 
 char* curPos = buffer;
 char* end = curPos + streamLength-1;
 char tempBuffer[1024];
 // Skip the <? ?>
	PRBool prevCharRight = PR_FALSE;
	while ( curPos < end )
	{
		if ( prevCharRight )
		{
			if (*curPos == '>' )
			{
			 curPos++;
			 break;
			}
			else
			{
				prevCharRight = PR_FALSE;
			}
		}
		else
		{
			if ( *curPos == '?' )
				prevCharRight = PR_TRUE;
		}
		curPos++;
	}
	
	while ( curPos < end )
	{
		// Find the next element
		while ( *curPos != '<' )
			curPos++;
		curPos++;
		
		PRInt32 tempPos =0;
		while (curPos < end && *curPos!=' ' )
		{
			tempBuffer[ tempPos++ ] = *curPos;
			curPos++;
		}
		curPos++;
		tempBuffer[tempPos] ='\n';
		
		if ( !nsCRT::strcmp( kMIMEType, tempBuffer ) )
		{
			rv = NS_ERROR_FAILURE;
			return rv;
		}
		
		nsCOMPtr<nsIMIMEInfo> info;
		rv = nsComponentManager::CreateInstance(NS_MIMEINFO_CONTRACTID, nsnull, nsIMIMEInfo::GetIID(),getter_AddRefs(info ));	  	
		if ( NS_FAILED( rv ) ) return rv;
		// Read in Attribute
		nsCString attribute;
		nsCString value;
		
		while ( curPos < end && !( *curPos=='\\' && *(curPos+1) !='>') )
		{
			if ( *curPos==' ' )
			{	
				 curPos++;
				 continue;
			}
			// Read Attribute
			tempPos =0;
			while ( curPos < end && *curPos!='=' )
			{
				tempBuffer[ tempPos++ ] = *curPos ++;
			}
			curPos++;
			tempBuffer[tempPos] ='\0';
			attribute = tempBuffer;
		// Read Value
			while ( curPos < end && *curPos ++ !='"' )
					;
			// Read Value
			tempPos =0;
			while ( curPos < end && *curPos!='"' )
			{
				tempBuffer[ tempPos++ ] = *curPos ++;
			}
			curPos++;
			tempBuffer[tempPos] ='\0';
			value = tempBuffer;
			AddAttribute( info, attribute, value );
	}
		curPos+=2;
		//Close
		rv = Add( info );
		if ( NS_FAILED( rv ) ) return rv;
	}
	
	mFile = aFile;

	return rv;
}
