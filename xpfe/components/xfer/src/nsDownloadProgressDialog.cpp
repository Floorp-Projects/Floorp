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
#include "nsDownloadProgressDialog.h"

#include "nsIAppShellComponentImpl.h"

#include "nsIServiceManager.h"
#include "nsIDocumentViewer.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsIContentViewer.h"
#include "nsIDOMElement.h"
#include "nsINetService.h"

static NS_DEFINE_IID( kAppShellServiceCID, NS_APPSHELL_SERVICE_CID );
static NS_DEFINE_IID( kNetServiceCID,      NS_NETSERVICE_CID );

static nsresult setAttribute( nsIDOMXULDocument *, const char*, const char*, const nsString& );

void
nsDownloadProgressDialog::OnClose() {
    // Close the window.
    if( mWindow ) {
        mWindow->Close();
    }
}

void
nsDownloadProgressDialog::OnStart() {
    // Load source stream into file.
    nsINetService *inet = 0;
    nsresult rv = nsServiceManager::GetService( kNetServiceCID,
                                                nsINetService::GetIID(),
                                                (nsISupports**)&inet );
    if (NS_OK == rv) {
        rv = inet->OpenStream( mUrl, this );
        nsServiceManager::ReleaseService( kNetServiceCID, inet );
    } else {
        DEBUG_PRINTF( PR_STDOUT, "%s %d: Error getting Net Service, rv=0x%X\n",
                      __FILE__, (int)__LINE__, (int)rv );
    }
}

void
nsDownloadProgressDialog::OnStop() {
    // Stop the netlib xfer.
    mStopped = PR_TRUE;
}

// ctor
nsDownloadProgressDialog::nsDownloadProgressDialog( nsIURL *aURL,
                                                    const nsFileSpec &anOutputFileName )
        : mUrl( nsDontQueryInterface<nsIURL>(aURL) ),
          mDocument(),
          mWindow(),
          mOutput(0),
          mFileName( anOutputFileName ),
          mBufLen( 8192 ),
          mBuffer( new char[ mBufLen ] ),
          mStopped( PR_FALSE ) {
    // Initialize ref count.
    NS_INIT_REFCNT();
}

// dtor
nsDownloadProgressDialog::~nsDownloadProgressDialog() {
    // Delete dynamically allocated members (file and buffer).
    delete mOutput;
    delete [] mBuffer;
}

// Open the dialog.
NS_IMETHODIMP
nsDownloadProgressDialog::Show() {
    nsresult rv = NS_OK;

    // Get app shell service.
    nsIAppShellService *appShell;
    rv = nsServiceManager::GetService( kAppShellServiceCID,
                                       nsIAppShellService::GetIID(),
                                       (nsISupports**)&appShell );

    if ( NS_SUCCEEDED( rv ) ) {
        // Open "download progress" dialog.
        nsIURL *url;
        rv = NS_NewURL( &url, "resource:/res/samples/downloadProgress.xul" );

        if ( NS_SUCCEEDED(rv) ) {
            // Create "save to disk" nsIXULCallbacks...
            nsIWebShellWindow *newWindow;
            rv = appShell->CreateTopLevelWindow( nsnull,
                                                 url,
                                                 PR_TRUE,
                                                 newWindow,
                                                 nsnull,
                                                 this,
                                                 0,
                                                 0 );
            if ( NS_SUCCEEDED( rv ) ) {
                mWindow = newWindow;
                NS_RELEASE( newWindow );
            } else {
                DEBUG_PRINTF( PR_STDOUT, "Error creating download progress dialog, rv=0x%X\n", (int)rv );
            }
            NS_RELEASE( url );
        }
        nsServiceManager::ReleaseService( kAppShellServiceCID, appShell );
    } else {
        DEBUG_PRINTF( PR_STDOUT, "Unable to get app shell service, rv=0x%X\n", (int)rv );
    }
    return rv;
}

// Do startup stuff from C++ side.
NS_IMETHODIMP
nsDownloadProgressDialog::ConstructBeforeJavaScript(nsIWebShell *aWebShell) {
    nsresult rv = NS_OK;

    // Get content viewer from the web shell.
    nsCOMPtr<nsIContentViewer> contentViewer;
    rv = aWebShell ? aWebShell->GetContentViewer(getter_AddRefs(contentViewer))
                   : NS_ERROR_NULL_POINTER;

    if ( contentViewer ) {
        // Up-cast to a document viewer.
        nsCOMPtr<nsIDocumentViewer> docViewer( do_QueryInterface( contentViewer, &rv ) );
        if ( docViewer ) {
            // Get the document from the doc viewer.
            nsCOMPtr<nsIDocument> document;
            rv = docViewer->GetDocument(*getter_AddRefs(document));
            if ( document ) {
                // Upcast to XUL document.
                mDocument = do_QueryInterface( document, &rv );
                if ( mDocument ) {
                    // Add ourself as document observer.
                    document->AddObserver( this );

                    // Store instance information into dialog's DOM.
                    const char *loc = 0;
                    mUrl->GetSpec( &loc );
                    rv = setAttribute( mDocument, "data.location", "value", loc );
                    if ( NS_SUCCEEDED( rv ) ) {
                        rv = setAttribute( mDocument, "data.fileName", "value", nsString((const char*)mFileName) );
                        if ( NS_SUCCEEDED( rv ) ) {
                            rv = setAttribute( mDocument, "dialog.start", "ready", "true" );
                        }
                    }
                } else {
                    DEBUG_PRINTF( PR_STDOUT, "%s %d: Upcast to nsIDOMXULDocument failed, rv=0x%X\n",
                                  __FILE__, (int)__LINE__, (int)rv );
                }
            } else {
                DEBUG_PRINTF( PR_STDOUT, "%s %d: GetDocument failed, rv=0x%X\n",
                              __FILE__, (int)__LINE__, (int)rv );
            }
        } else {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: Upcast to nsIDocumentViewer failed, rv=0x%X\n",
                          __FILE__, (int)__LINE__, (int)rv );
        }
    } else {
        DEBUG_PRINTF( PR_STDOUT, "%s %d: GetContentViewer failed, rv=0x%X\n",
                      __FILE__, (int)__LINE__, (int)rv );
    }

    return rv;
}

NS_IMETHODIMP
nsDownloadProgressDialog::OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength) {
    nsresult rv = NS_OK;

    // Check for download cancelled by user.
    if ( mStopped ) {
        // Close the output file.
        if ( mOutput ) {
            mOutput->close();
        }
        // Close the input stream.
        aIStream->Close();
    } else {
        // Allocate buffer space.
        if ( aLength > mBufLen ) {
            char *oldBuffer = mBuffer;
    
            mBuffer = new char[ aLength ];
    
            if ( mBuffer ) {
                // Use new (bigger) buffer.
                mBufLen = aLength;
                // Delete old (smaller) buffer.
                delete [] oldBuffer;
            } else {
                // Keep the one we've got.
                mBuffer = oldBuffer;
            }
        }
    
        // Read the data.
        PRUint32 bytesRead;
        rv = aIStream->Read( mBuffer, ( mBufLen > aLength ) ? aLength : mBufLen, &bytesRead );
    
        if ( NS_SUCCEEDED(rv) ) {
            // Write the data just read to the output stream.
            if ( mOutput ) {
                mOutput->write( mBuffer, bytesRead );
            }
        } else {
            printf( "Error reading stream, rv=0x%X\n", (int)rv );
        }
    }

    return rv;
}

NS_IMETHODIMP
nsDownloadProgressDialog::OnStartBinding(nsIURL* aURL, const char *aContentType) {
    nsresult rv = NS_OK;
    return NS_OK;
}

NS_IMETHODIMP
nsDownloadProgressDialog::OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax) {
    nsresult rv = NS_OK;
    char buf[16];
    PR_snprintf( buf, sizeof buf, "%lu", aProgressMax );
    setAttribute( mDocument, "data.progress", "max", buf );
    PR_snprintf( buf, sizeof buf, "%lu", aProgress );
    setAttribute( mDocument, "data.progress", "value", buf );
    return NS_OK;
}

NS_IMETHODIMP
nsDownloadProgressDialog::OnStatus(nsIURL* aURL, const PRUnichar* aMsg) {
    nsresult rv = NS_OK;
    nsString msg = aMsg;
    setAttribute( mDocument, "data.status", "value", aMsg );
    return NS_OK;
}

NS_IMETHODIMP
nsDownloadProgressDialog::OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg) {
    nsresult rv = NS_OK;
    // Close the output file.
    if ( mOutput ) {
        mOutput->close();
    }
    // Signal UI that download is complete.
    setAttribute( mDocument, "data.progress", "completed", "true" );
    return rv;
}

// Handle attribute changing; we only care about the element "data.execute"
// which is used to signal command execution from the UI.
NS_IMETHODIMP
nsDownloadProgressDialog::AttributeChanged( nsIDocument *aDocument,
                                        nsIContent*  aContent,
                                        nsIAtom*     aAttribute,
                                        PRInt32      aHint ) {
    nsresult rv = NS_OK;
    // Look for data.execute command changing.
    nsString id;
    nsCOMPtr<nsIAtom> atomId = nsDontQueryInterface<nsIAtom>( NS_NewAtom("id") );
    aContent->GetAttribute( kNameSpaceID_None, atomId, id );
    if ( id == "data.execute" ) {
        nsString cmd;
        nsCOMPtr<nsIAtom> atomCommand = nsDontQueryInterface<nsIAtom>( NS_NewAtom("command") );
        // Get requested command.
        aContent->GetAttribute( kNameSpaceID_None, atomCommand, cmd );
        // Reset (immediately, to prevent feedback loop).
        aContent->SetAttribute( kNameSpaceID_None, atomCommand, "", PR_FALSE );
        if ( 0 ) {
        } else if ( cmd == "start" ) {
            OnStart();
        } else if ( cmd == "stop" ) {
            OnStop();
        } else if ( cmd == "close" ) {
            OnClose();
        } else {
        }
    }

    return rv;
}

// Standard implementations of addref/release/query_interface.
NS_IMPL_ADDREF( nsDownloadProgressDialog );
NS_IMPL_RELEASE( nsDownloadProgressDialog );

NS_IMETHODIMP 
nsDownloadProgressDialog::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if (aIID.Equals(nsIDocumentObserver::GetIID())) {
    *aInstancePtr = (void*) ((nsIDocumentObserver*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIXULWindowCallbacks::GetIID())) {
    *aInstancePtr = (void*) ((nsIXULWindowCallbacks*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}

// Utility to set element attribute.
static nsresult setAttribute( nsIDOMXULDocument *doc,
                              const char *id,
                              const char *name,
                              const nsString &value ) {
    nsresult rv = NS_OK;

    if ( doc ) {
        // Find specified element.
        nsCOMPtr<nsIDOMElement> elem;
        rv = doc->GetElementById( id, getter_AddRefs( elem ) );
        if ( elem ) {
            // Set the text attribute.
            rv = elem->SetAttribute( name, value );
            if ( NS_SUCCEEDED( rv ) ) {
            } else {
                 DEBUG_PRINTF( PR_STDOUT, "%s %d: SetAttribute failed, rv=0x%X\n",
                               __FILE__, (int)__LINE__, (int)rv );
            }
        } else {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: GetElementById failed, rv=0x%X\n",
                          __FILE__, (int)__LINE__, (int)rv );
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}
