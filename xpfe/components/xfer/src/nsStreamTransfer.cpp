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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsIStreamTransfer.h"

#include "nsIAppShellComponentImpl.h"
#include "nsStreamXferOp.h"
#include "nsIFileSpecWithUI.h"
#include "nsNetUtil.h"
#include "nsIPref.h"
#include "nsIURL.h"
#include "nsEscape.h"

// {BEBA91C0-070F-11d3-8068-00600811A9C3}
#define NS_STREAMTRANSFER_CID \
    { 0xbeba91c0, 0x70f, 0x11d3, { 0x80, 0x68, 0x0, 0x60, 0x8, 0x11, 0xa9, 0xc3 } }

// Implementation of the stream transfer component interface.
class nsStreamTransfer : public nsIStreamTransfer,
                         public nsAppShellComponentImpl {
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

    // This class implements the nsIAppShellComponent interface functions.
    NS_DECL_NSIAPPSHELLCOMPONENT

    // This class implements the nsIStreamTransfer interface functions.
    NS_DECL_NSISTREAMTRANSFER

private:
    // Put up file picker dialog.
    NS_IMETHOD SelectFile( nsIFileSpec **result, const nsCString &suggested );
    nsCString  SuggestNameFor( nsIChannel *aChannel );

    // Objects of this class are counted to manage library unloading...
    nsInstanceCounter instanceCounter;
}; // nsStreamTransfer

NS_IMETHODIMP
nsStreamTransfer::SelectFileAndTransferLocation( nsIChannel *aChannel, nsIDOMWindow *parent ) {
    // Prompt the user for the destination file.
    nsCOMPtr<nsIFileSpec> outputFile;
    PRBool isValid = PR_FALSE;
    nsresult rv = SelectFile( getter_AddRefs( outputFile ),
                              SuggestNameFor( aChannel ).GetBuffer() );

    if ( NS_SUCCEEDED( rv )
         &&
         outputFile
         &&
         NS_SUCCEEDED( outputFile->IsValid( &isValid ) )
         &&
         isValid ) {
        // Construct stream transfer operation to be given to dialog.
        nsStreamXferOp *p= new nsStreamXferOp( aChannel, outputFile );

        if ( p ) {
            // Open download progress dialog.
            NS_ADDREF(p);
            rv = p->OpenDialog( parent );
            NS_RELEASE(p);
            if ( NS_FAILED( rv ) ) {
                DEBUG_PRINTF( PR_STDOUT, "%s %d : Error opening dialog, rv=0x%08X\n",
                              (char *)__FILE__, (int)__LINE__, (int)rv );
            }
        } else {
            DEBUG_PRINTF( PR_STDOUT, "%s %d : Unable to create nsStreamXferOp\n",
                          (char *)__FILE__, (int)__LINE__ );
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        if ( NS_FAILED( rv ) ) {
            DEBUG_PRINTF( PR_STDOUT, "Failed to select file, rv=0x%X\n", (int)rv );
        } else {
            // User cancelled.
        }
    }

    return rv;
}

NS_IMETHODIMP
nsStreamTransfer::SelectFileAndTransferLocationSpec( char const *aURL, nsIDOMWindow *parent ) {
    nsresult rv = NS_OK;

    // Construct URI from spec.
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI( getter_AddRefs( uri ), aURL );

    if ( NS_SUCCEEDED( rv ) && uri ) {
        // Construct channel from URI.
        nsCOMPtr<nsIChannel> channel;
        rv = NS_OpenURI( getter_AddRefs( channel ), uri, nsnull );

        if ( NS_SUCCEEDED( rv ) && channel ) {
            // Transfer channel to output file chosen by user.
            rv = this->SelectFileAndTransferLocation( channel, parent );
        } else {
            DEBUG_PRINTF( PR_STDOUT, "Failed to open URI, rv=0x%X\n", (int)rv );
        }
    } else {
        DEBUG_PRINTF( PR_STDOUT, "Failed to create URI, rv=0x%X\n", (int)rv );
    }

    return rv;
}

NS_IMETHODIMP
nsStreamTransfer::SelectFile( nsIFileSpec **aResult, const nsCString &suggested ) {
    nsresult rv = NS_OK;

    if ( aResult ) {
        *aResult = 0;

        // Prompt user for file name.
        nsCOMPtr<nsIFileSpecWithUI> result;
        result = getter_AddRefs( NS_CreateFileSpecWithUI() );
      
        if ( result ) {
            // Prompt for file name.
            nsCOMPtr<nsIFileSpec> startDir;

            // Pull in the user's preferences and get the default download directory.
            NS_WITH_SERVICE( nsIPref, prefs, NS_PREF_PROGID, &rv );
            if ( NS_SUCCEEDED( rv ) && prefs ) {
                prefs->GetFilePref( "browser.download.dir", getter_AddRefs( startDir ) );
                if ( startDir ) {
                    PRBool isValid = PR_FALSE;
                    startDir->IsValid( &isValid );
                    if ( isValid ) {
                        // Set result so startDir is used.
                        result->FromFileSpec( startDir );
                    }
                }
            }

            //XXX l10n
            nsAutoCString title(NS_ConvertASCIItoUCS2("Save File"));
        
            rv = result->ChooseOutputFile( title,
                                           suggested.IsEmpty() ? 0 : suggested.GetBuffer(),
                                           nsIFileSpecWithUI::eAllFiles );

            if ( NS_SUCCEEDED( rv ) ) {
                // Give result to caller.
                rv = result->QueryInterface( NS_GET_IID(nsIFileSpec), (void**)aResult );

                if ( NS_SUCCEEDED( rv ) && prefs ) {
                    // Save selected directory for next time.
                    rv = result->GetParent( getter_AddRefs( startDir ) );
                    if ( NS_SUCCEEDED( rv ) && startDir ) {
                        prefs->SetFilePref( "browser.download.dir", startDir, PR_FALSE );
                    }
                }
            }
        } else {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: Error creating file widget, rv=0x%X\n",
                          __FILE__, (int)__LINE__, (int)rv );
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

nsCString nsStreamTransfer::SuggestNameFor( nsIChannel *aChannel ) {
    nsCString result;
    if ( aChannel ) {
        // Get URI from channel and spec from URI.
        nsCOMPtr<nsIURI> uri;
        nsresult rv = aChannel->GetURI( getter_AddRefs( uri ) );
        if ( NS_SUCCEEDED( rv ) && uri ) {
            // Try to get URL from URI.
            nsCOMPtr<nsIURL> url( do_QueryInterface( uri, &rv ) );
            if ( NS_SUCCEEDED( rv ) && url ) {
                char *nameFromURL = 0;
                rv = url->GetFileName( &nameFromURL );
                if ( NS_SUCCEEDED( rv ) && nameFromURL ) {
                    // Unescape the file name (GetFileName escapes it).
                    result = nsUnescape( nameFromURL );
                    nsCRT::free( nameFromURL );
                }
            }
        }
    }
    return result;
}

// Generate base nsIAppShellComponent implementation.
NS_IMPL_IAPPSHELLCOMPONENT( nsStreamTransfer,
                            nsIStreamTransfer,
                            NS_ISTREAMTRANSFER_PROGID,
                            0 )
