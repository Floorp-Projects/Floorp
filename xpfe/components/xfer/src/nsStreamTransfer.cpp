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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsIStreamTransfer.h"

#include "nsStreamXferOp.h"
#include "nsIFilePicker.h"
#include "nsILocalFile.h"
#include "nsIFileChannel.h"
#include "nsNetUtil.h"
#include "nsIPref.h"
#include "nsIURL.h"
#include "nsEscape.h"
#include "nsIHttpChannel.h"
#include "nsIUploadChannel.h"
#include "nsICachingChannel.h"
#include "nsIStringBundle.h"
#include "nsIAllocator.h"
#include "nsIFileStream.h"

#include "nsIGenericFactory.h"


// {BEBA91C0-070F-11d3-8068-00600811A9C3}
#define NS_STREAMTRANSFER_CID \
    { 0xbeba91c0, 0x70f, 0x11d3, { 0x80, 0x68, 0x0, 0x60, 0x8, 0x11, 0xa9, 0xc3 } }

// Implementation of the stream transfer component interface.
class nsStreamTransfer : public nsIStreamTransfer {
public:
    NS_DEFINE_STATIC_CID_ACCESSOR( NS_STREAMTRANSFER_CID );

    // ctor/dtor
    nsStreamTransfer() {
        NS_INIT_REFCNT();
    }
    virtual ~nsStreamTransfer() {
    }

    // This class implements the nsISupports interface functions.
    NS_DECL_ISUPPORTS

    // This class implements the nsIStreamTransfer interface functions.
    NS_DECL_NSISTREAMTRANSFER

protected:
    // Common implementation that takes contentType and suggestedName.
    NS_IMETHOD SelectFileAndTransferLocation( nsIChannel *aChannel,
                                              nsIDOMWindowInternal *parent,
                                              char const *contentType,
                                              char const *suggestedName );

private:
    // Put up file picker dialog.
    NS_IMETHOD SelectFile( nsIDOMWindowInternal *parent, nsILocalFile **result, const nsString &suggested );
    nsString  SuggestNameFor( nsIChannel *aChannel, char const *suggestedName );

}; // nsStreamTransfer

NS_IMPL_ISUPPORTS1(nsStreamTransfer, nsIStreamTransfer)

// Get content type and suggested name from input channel in this case.
NS_IMETHODIMP
nsStreamTransfer::SelectFileAndTransferLocation( nsIChannel *aChannel, nsIDOMWindowInternal *parent ) {
    
    nsCOMPtr<nsIURI> uri;
    nsresult rv = aChannel->GetURI( getter_AddRefs( uri ) );
    if (NS_FAILED(rv)) return rv;

    // Content type comes straight from channel.
    nsXPIDLCString contentType;
    aChannel->GetContentType( getter_Copies( contentType ) );
    
    // Suggested name derived from content-disposition response header.
    nsCAutoString suggestedName;

    // Try to get HTTP channel.
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface( aChannel );
    if ( httpChannel ) {
        // Get content-disposition response header.
        nsXPIDLCString disp; 
        rv = httpChannel->GetResponseHeader( "content-disposition", getter_Copies( disp ) );
        if ( NS_SUCCEEDED( rv ) && disp ) {
            // Parse out file name.
            nsCAutoString contentDisp(NS_STATIC_CAST(const char*, disp));
            // Remove whitespace.
            contentDisp.StripWhitespace();
            // Look for ";filename=".
            char key[] = ";filename=";
            PRInt32 i = contentDisp.Find( key );
            if ( i != kNotFound ) {
                // Name comes after that.
                suggestedName = contentDisp.get() + i + PL_strlen( key ) + 1;
            }
        }
    }
    return SelectFileAndTransferLocation( aChannel, parent, contentType.get(), suggestedName.get() );
}

NS_IMETHODIMP
nsStreamTransfer::SelectFileAndTransferLocation( nsIChannel *aChannel,
                                                 nsIDOMWindowInternal *parent,
                                                 char const *contentType,
                                                 char const *suggestedName ) {
    // Prompt the user for the destination file.
    nsCOMPtr<nsILocalFile> outputFile;
    nsresult rv = SelectFile( parent, 
                              getter_AddRefs( outputFile ),
                              SuggestNameFor( aChannel, suggestedName ) );

    if ( NS_SUCCEEDED( rv )
         &&
         outputFile ) {
        // Try to get HTTP channel.
        nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface( aChannel );
        if ( httpChannel ) {
            // Turn off content encoding conversions.
            httpChannel->SetApplyConversion( PR_FALSE );
        }

        // Construct stream transfer operation to be given to dialog.
        nsStreamXferOp *p= new nsStreamXferOp( aChannel, outputFile );

        if ( p ) {
            // Open download progress dialog.
            NS_ADDREF(p);
            rv = p->OpenDialog( parent );
            NS_RELEASE(p);
            NS_ASSERTION(NS_SUCCEEDED(rv), "Error opening dialog");
        } else {
          NS_ASSERTION(0, "Unable to create nsStreamXferOp");
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    }

    return rv;
}

NS_IMETHODIMP
nsStreamTransfer::SelectFileAndTransferLocationSpec( char const *aURL,
                                                     nsIDOMWindowInternal *parent,
                                                     char const *contentType,
                                                     char const *suggestedName,
                                                     PRBool      doNotValidate,
                                                     nsIInputStream *postData,
                                                     nsISupports *cacheKey ) {
    nsresult rv = NS_OK;

    // Construct URI from spec.
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI( getter_AddRefs( uri ), aURL );

    if ( NS_SUCCEEDED( rv ) && uri ) {
        // Construct channel from URI.
        nsCOMPtr<nsIChannel> channel;
        rv = NS_OpenURI( getter_AddRefs( channel ), uri, nsnull );

        if ( NS_SUCCEEDED( rv ) && channel ) {
            // See if LOAD_FROM_CACHE is called for.
            if ( doNotValidate ) {
                channel->SetLoadFlags( nsIRequest::LOAD_FROM_CACHE );
                if ( cacheKey ) {
                  nsCOMPtr<nsICachingChannel> cachingChannel(do_QueryInterface(channel));
                  if (cachingChannel) {
                    // Say we want it out of the cache only if we have
                    // post data. If the stream has already been evicted,
                    // we'll put up a dialog before reposting.
                    cachingChannel->SetCacheKey(cacheKey, 
                                                (postData != nsnull));
                  }
                }
            }
            // Post data provided?
            if ( postData ) {
                // See if it's an http channel.
                nsCOMPtr<nsIHttpChannel> httpChannel( do_QueryInterface( channel ) );
                if ( httpChannel ) {
                    // Rewind stream and attach to channel.
                    nsCOMPtr<nsISeekableStream> stream( do_QueryInterface( postData ) );
                    if ( stream ) {
                        stream->Seek( nsISeekableStream::NS_SEEK_SET, 0 );
                        nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(httpChannel));
                        NS_ASSERTION(uploadChannel, "http must support nsIUploadChannel");

                        uploadChannel->SetUploadStream( postData, nsnull, -1);
                        httpChannel->SetRequestMethod("POST");
                    }
                }
            }
            // Transfer channel to output file chosen by user.
            rv = this->SelectFileAndTransferLocation( channel, parent, contentType, suggestedName );
        } else {
          NS_ASSERTION(0,"Failed to open URI");
        }
    } else {
      NS_WARNING("Failed to create URI");
    }

    return rv;
}

NS_IMETHODIMP
nsStreamTransfer::SelectFile( nsIDOMWindowInternal *parent, nsILocalFile **aResult, const nsString &suggested ) {
    nsresult rv = NS_OK;

    if ( aResult ) {
        *aResult = 0;

        // Prompt user for file name.
        nsCOMPtr<nsIFilePicker> picker = do_CreateInstance( "@mozilla.org/filepicker;1" );
      
        if ( picker ) {
            // Prompt for file name.
            nsCOMPtr<nsILocalFile> startDir;

            // Pull in the user's preferences and get the default download directory.
            nsCOMPtr<nsIPref> prefs = 
                     do_GetService( NS_PREF_CONTRACTID, &rv );
            if ( NS_SUCCEEDED( rv ) && prefs ) {
                prefs->GetFileXPref( "browser.download.dir", getter_AddRefs( startDir ) );
                if ( startDir ) {
                    PRBool isValid = PR_FALSE;
                    startDir->Exists( &isValid );
                    if ( isValid ) {
                      // Set file picker so startDir is used.
                      picker->SetDisplayDirectory( startDir );
                    }
                }
            }

            nsAutoString title( NS_ConvertASCIItoUCS2( "Save File" ) );
            nsCID cid = NS_STRINGBUNDLESERVICE_CID;
            nsCOMPtr<nsIStringBundleService> bundleService = 
                     do_GetService( cid, &rv );
            if ( NS_SUCCEEDED( rv ) ) {
                nsCOMPtr<nsIStringBundle> bundle;
                PRUnichar *pString;
                rv = bundleService->CreateBundle( "chrome://global/locale/downloadProgress.properties",
                                                  getter_AddRefs(bundle) );
                if ( NS_SUCCEEDED( rv ) ) {
                    rv = bundle->GetStringFromName( NS_ConvertASCIItoUCS2( "FilePickerTitle" ).get(),
                                                    &pString );
                    if ( NS_SUCCEEDED( rv ) && pString ) {
                        title = pString;
                        nsAllocator::Free( pString );
                    }
                }
            }
        
            rv = picker->Init( parent, title.get(), nsIFilePicker::modeSave );
            PRInt16 rc = nsIFilePicker::returnCancel;
            if ( NS_SUCCEEDED( rv ) ) {
                // Set default file name.
                rv = picker->SetDefaultString( suggested.get() );

                // Set file filter mask.
                rv = picker->AppendFilters( nsIFilePicker::filterAll );

                rv = picker->Show( &rc );
            }

            if ( rc != nsIFilePicker::returnCancel ) {
                // Give result to caller.
                rv = picker->GetFile( aResult );

                if ( NS_SUCCEEDED( rv ) && prefs ) {
                    // Save selected directory for next time.
                    nsCOMPtr<nsIFile> newStartDir;
                    rv = (*aResult)->GetParent( getter_AddRefs( newStartDir ) );

                    startDir = do_QueryInterface( newStartDir );

                    if ( NS_SUCCEEDED( rv ) && startDir ) {
                        prefs->SetFileXPref( "browser.download.dir", startDir );
                    }
                }
            } else if ( NS_SUCCEEDED( rv ) ) {
                // User cancelled.
                rv = NS_ERROR_ABORT;
            }
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

// Guess a save-as file name from channel (URL) and/or "suggested name" (which likely
// came from content-disposition response header).
nsString nsStreamTransfer::SuggestNameFor( nsIChannel *aChannel, char const *suggestedName ) {
    nsString result;
    if ( *suggestedName ) {
        // Exclude any path information from this!  This is mandatory as
        // this suggested name comes from a http response header and could
        // try to overwrite c:\config.sys or something.
        nsCOMPtr<nsILocalFile> localFile;
        nsCAutoString suggestedFileName(suggestedName);
        if ( NS_SUCCEEDED( NS_NewLocalFile( nsUnescape(NS_CONST_CAST(char*, suggestedFileName.get())), PR_FALSE, getter_AddRefs( localFile ) ) ) ) {
            // We want base part of name only.
            nsXPIDLString baseName;
            if ( NS_SUCCEEDED( localFile->GetUnicodeLeafName( getter_Copies( baseName ) ) ) ) {
                result = baseName.get();
            }
        }
    } else if ( aChannel ) {
        // Get URI from channel and spec from URI.
        nsCOMPtr<nsIURI> uri;
        nsresult rv = aChannel->GetURI( getter_AddRefs( uri ) );
        if ( NS_SUCCEEDED( rv ) && uri ) {
            // Try to get URL from URI.
            nsCOMPtr<nsIFileURL> fileurl( do_QueryInterface( uri, &rv ) );
            if ( NS_SUCCEEDED( rv ) && fileurl)
            {
              nsCOMPtr<nsIFile> localeFile;
              rv = fileurl->GetFile(getter_AddRefs(localeFile));
              if ( NS_SUCCEEDED( rv ) && localeFile)
              {
                nsXPIDLString baseName;
                if ( NS_SUCCEEDED( localeFile->GetUnicodeLeafName( getter_Copies( baseName ) ) ) ) {
                  result = baseName.get();
                  return result;
                }
              }
            }

            nsCOMPtr<nsIURL> url( do_QueryInterface( uri, &rv ) );
            if ( NS_SUCCEEDED( rv ) && url ) {
                char *nameFromURL = 0;
                rv = url->GetFileName( &nameFromURL );
                if ( NS_SUCCEEDED( rv ) && nameFromURL ) {
                    // Unescape the file name (GetFileName escapes it).
                    nsUnescape( nameFromURL );

                    //make sure nameFromURL only contains ascii,
                    //otherwise we have no idea about urlname's original charset, suggest nothing
                    char *ptr;
                    for (ptr = nameFromURL; *ptr; ptr++)
                      if (*ptr & '\200')
                        break;

                    if (!(*ptr))
                      result = NS_ConvertASCIItoUCS2(nameFromURL).get();

                    nsCRT::free( nameFromURL );
                }
            }
        }
    }
    return result;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsStreamTransfer)

static nsModuleComponentInfo components[] = {
  { NS_ISTREAMTRANSFER_CLASSNAME,
    NS_STREAMTRANSFER_CID,
    NS_ISTREAMTRANSFER_CONTRACTID,
    nsStreamTransferConstructor}
};


NS_IMPL_NSGETMODULE(nsStreamTransferModule, components);

