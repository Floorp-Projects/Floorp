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
#include "nsIFilePicker.h"
#include "nsILocalFile.h"
#include "nsNetUtil.h"
#include "nsIPref.h"
#include "nsIURL.h"
#include "nsEscape.h"
#include "nsIHTTPChannel.h"
#include "nsIStringBundle.h"
#include "nsIAllocator.h"
#include "nsIFileStream.h"

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

protected:
    // Common implementation that takes contentType and suggestedName.
    NS_IMETHOD SelectFileAndTransferLocation( nsIChannel *aChannel,
                                              nsIDOMWindowInternal *parent,
                                              char const *contentType,
                                              char const *suggestedName );

private:
    // Put up file picker dialog.
    NS_IMETHOD SelectFile( nsIDOMWindowInternal *parent, nsIFileSpec **result, const nsCString &suggested );
    nsCString  SuggestNameFor( nsIChannel *aChannel, char const *suggestedName );

    // Objects of this class are counted to manage library unloading...
    nsInstanceCounter instanceCounter;
}; // nsStreamTransfer

// Get content type and suggested name from input channel in this case.
NS_IMETHODIMP
nsStreamTransfer::SelectFileAndTransferLocation( nsIChannel *aChannel, nsIDOMWindowInternal *parent ) {
    // Content type comes straight from channel.
    nsXPIDLCString contentType;
    aChannel->GetContentType( getter_Copies( contentType ) );

    // Suggested name derived from content-disposition response header.
    nsCAutoString suggestedName;

    // Try to get HTTP channel.
    nsCOMPtr<nsIHTTPChannel> httpChannel = do_QueryInterface( aChannel );
    if ( httpChannel ) {
        // Get content-disposition response header.
        nsCOMPtr<nsIAtom> atom = NS_NewAtom( "content-disposition" );
        if ( atom ) {
            nsXPIDLCString disp; 
            nsresult rv = httpChannel->GetResponseHeader( atom, getter_Copies( disp ) );
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
                    suggestedName = contentDisp.GetBuffer() + i + PL_strlen( key ) + 1;
                }
            }
        }
    }
    return SelectFileAndTransferLocation( aChannel, parent, contentType, suggestedName );
}

NS_IMETHODIMP
nsStreamTransfer::SelectFileAndTransferLocation( nsIChannel *aChannel,
                                                 nsIDOMWindowInternal *parent,
                                                 char const *contentType,
                                                 char const *suggestedName ) {
    // Prompt the user for the destination file.
    nsCOMPtr<nsIFileSpec> outputFile;
    PRBool isValid = PR_FALSE;
    nsresult rv = SelectFile( parent, 
                              getter_AddRefs( outputFile ),
                              SuggestNameFor( aChannel, suggestedName ) );

    if ( NS_SUCCEEDED( rv )
         &&
         outputFile
         &&
         NS_SUCCEEDED( outputFile->IsValid( &isValid ) )
         &&
         isValid ) {
        // Try to get HTTP channel.
        nsCOMPtr<nsIHTTPChannel> httpChannel = do_QueryInterface( aChannel );
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
nsStreamTransfer::SelectFileAndTransferLocationSpec( char const *aURL,
                                                     nsIDOMWindowInternal *parent,
                                                     char const *contentType,
                                                     char const *suggestedName,
                                                     PRBool      doNotValidate,
                                                     nsIInputStream *postData ) {
    nsresult rv = NS_OK;

    // Construct URI from spec.
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI( getter_AddRefs( uri ), aURL );

    if ( NS_SUCCEEDED( rv ) && uri ) {
        // Construct channel from URI.
        nsCOMPtr<nsIChannel> channel;
        rv = NS_OpenURI( getter_AddRefs( channel ), uri, nsnull );

        if ( NS_SUCCEEDED( rv ) && channel ) {
            // See if VALIDATE_NEVER is called for.
            if ( doNotValidate ) {
                channel->SetLoadAttributes( nsIChannel::VALIDATE_NEVER );
            }
            // Post data provided?
            if ( postData ) {
                // See if it's an http channel.
                nsCOMPtr<nsIHTTPChannel> httpChannel( do_QueryInterface( channel ) );
                if ( httpChannel ) {
                    // Rewind stream and attach to channel.
                    nsCOMPtr<nsIRandomAccessStore> stream( do_QueryInterface( postData ) );
                    if ( stream ) {
                        stream->Seek( PR_SEEK_SET, 0 );
                        httpChannel->SetUploadStream( postData );
                        nsCOMPtr<nsIAtom> method = NS_NewAtom ("POST");
                        httpChannel->SetRequestMethod(method);
                    }
                }
            }
            // Transfer channel to output file chosen by user.
            rv = this->SelectFileAndTransferLocation( channel, parent, contentType, suggestedName );
        } else {
            DEBUG_PRINTF( PR_STDOUT, "Failed to open URI, rv=0x%X\n", (int)rv );
        }
    } else {
        DEBUG_PRINTF( PR_STDOUT, "Failed to create URI, rv=0x%X\n", (int)rv );
    }

    return rv;
}

NS_IMETHODIMP
nsStreamTransfer::SelectFile( nsIDOMWindowInternal *parent, nsIFileSpec **aResult, const nsCString &suggested ) {
    nsresult rv = NS_OK;

    if ( aResult ) {
        *aResult = 0;

        // Prompt user for file name.
        nsCOMPtr<nsIFilePicker> picker = do_CreateInstance( "component://mozilla/filepicker" );
      
        if ( picker ) {
            // Prompt for file name.
            nsCOMPtr<nsILocalFile> startDir;

            // Pull in the user's preferences and get the default download directory.
            NS_WITH_SERVICE( nsIPref, prefs, NS_PREF_PROGID, &rv );
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
            NS_WITH_SERVICE( nsIStringBundleService, bundleService, cid, &rv );
            if ( NS_SUCCEEDED( rv ) ) {
                nsILocale *locale = 0;
                nsIStringBundle *bundle;
                PRUnichar *pString;
                rv = bundleService->CreateBundle( "chrome://global/locale/downloadProgress.properties",
                                                  locale, 
                                                  getter_AddRefs( &bundle ) );
                if ( NS_SUCCEEDED( rv ) ) {
                    rv = bundle->GetStringFromName( NS_ConvertASCIItoUCS2( "FilePickerTitle" ).GetUnicode(),
                                                    &pString );
                    if ( NS_SUCCEEDED( rv ) && pString ) {
                        title = pString;
                        nsAllocator::Free( pString );
                    }
                }
            }
        
            rv = picker->Init( parent, title.GetUnicode(), nsIFilePicker::modeSave );
            PRInt16 rc = nsIFilePicker::returnCancel;
            if ( NS_SUCCEEDED( rv ) ) {
                // Set default file name.
                rv = picker->SetDefaultString( NS_ConvertASCIItoUCS2( suggested.GetBuffer() ).GetUnicode() );

                // Set file filter mask.
                rv = picker->AppendFilters( nsIFilePicker::filterAll );

                rv = picker->Show( &rc );
            }

            #ifdef DEBUG_law
            printf( "\nFile picker result = 0x%04X\n\n", (int)rc );
            #endif
            if ( rc != nsIFilePicker::returnCancel ) {
                // Give result to caller.
                nsCOMPtr<nsILocalFile> selection;
                if ( NS_SUCCEEDED( picker->GetFile( getter_AddRefs( selection ) ) ) ) {
                    nsXPIDLCString selectionPath;
                    if ( NS_SUCCEEDED( selection->GetPath( getter_Copies( selectionPath ) ) ) ) {
                        rv = NS_NewFileSpec( aResult );
                        if ( NS_SUCCEEDED( rv ) ) {
                            rv = (*aResult)->SetNativePath( selectionPath );
                            printf( "\nresult native path = %s\n\n", (const char *)selectionPath );
                        }
                    }
                }

                if ( NS_SUCCEEDED( rv ) && prefs ) {
                    // Save selected directory for next time.
                    nsCOMPtr<nsIFileSpec> startDirPath;
                    rv = (*aResult)->GetParent( getter_AddRefs(startDirPath));

                    // go through nsFileSpec to get to nsILocalFile
                    nsFileSpec startDirSpec;

                    if (NS_SUCCEEDED(rv))
                      rv = startDirPath->GetFileSpec(&startDirSpec);
                    
                    if (NS_SUCCEEDED(rv))
                      rv = NS_FileSpecToIFile(&startDirSpec,
                                              getter_AddRefs(startDir));
                    
                    if ( NS_SUCCEEDED( rv ) && startDir ) {
                        prefs->SetFileXPref( "browser.download.dir", startDir);
                        #ifdef DEBUG_law
                        printf( "\nbrowser.download.dir has been reset\n\n" );
                        #endif
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
nsCString nsStreamTransfer::SuggestNameFor( nsIChannel *aChannel, char const *suggestedName ) {
    nsCString result(suggestedName);
    if ( !result.IsEmpty() ) {
        // Exclude any path information from this!  This is mandatory as
        // this suggested name comes from a http response header and could
        // try to overwrite c:\config.sys or something.
        nsCOMPtr<nsILocalFile> localFile;
        if ( NS_SUCCEEDED( NS_NewLocalFile( result, PR_FALSE, getter_AddRefs( localFile ) ) ) ) {
            // We want base part of name only.
            nsXPIDLCString baseName;
            if ( NS_SUCCEEDED( localFile->GetLeafName( getter_Copies( baseName ) ) ) ) {
                // Unescape this for display in dialog.
                result = nsUnescape( (char*)(const char*)baseName );
            }
        }
    } else if ( aChannel ) {
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
