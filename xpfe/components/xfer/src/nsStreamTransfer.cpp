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
#include "nsIStreamTransfer.h"

#include "nsIAppShellComponentImpl.h"
#include "nsStreamXferOp.h"
#include "nsIFileWidget.h"
#include "nsWidgetsCID.h"
#include "nsIURL.h"
#ifdef NECKO
#include "nsNeckoUtil.h"
#endif

// {BEBA91C0-070F-11d3-8068-00600811A9C3}
#define NS_STREAMTRANSFER_CID \
    { 0xbeba91c0, 0x70f, 0x11d3, { 0x80, 0x68, 0x0, 0x60, 0x8, 0x11, 0xa9, 0xc3 } }

static NS_DEFINE_IID( kCFileWidgetCID, NS_FILEWIDGET_CID  );
static NS_DEFINE_IID( kIFileWidgetIID, NS_IFILEWIDGET_IID );

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
    NS_IMETHOD SelectFile( nsFileSpec &result );

    // Objects of this class are counted to manage library unloading...
    nsInstanceCounter instanceCounter;
}; // nsStreamTransfer

NS_IMETHODIMP
nsStreamTransfer::SelectFileAndTransferLocation( nsIURI *aURL, nsIDOMWindow *parent ) {
    // Prompt the user for the destination file.
    nsFileSpec outputFileName;
    nsresult rv = SelectFile( outputFileName );

    if ( NS_SUCCEEDED( rv ) ) {
        // Open a downloadProgress dialog.
#ifdef NECKO
        char *source = 0;
#else
        const char *source = 0;
#endif
        aURL->GetSpec( &source );

        nsStreamXferOp *p= new nsStreamXferOp( source, (const char*)outputFileName );
        nsCOMPtr<nsIStreamTransferOperation> op = dont_QueryInterface( (nsIStreamTransferOperation*)p ); 

#ifdef NECKO
        nsCRT::free( source );
#endif

        if ( op ) {
            // Open download progress dialog.
            rv = p->OpenDialog( parent );
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
        DEBUG_PRINTF( PR_STDOUT, "Failed to select file, rv=0x%X\n", (int)rv );
    }

    return rv;
}

NS_IMETHODIMP
nsStreamTransfer::SelectFileAndTransferLocationSpec( char const *aURL, nsIDOMWindow *parent ) {
    nsresult rv = NS_OK;

    // Construct URI from spec.
    nsIURI *uri;
#ifndef NECKO
    rv = NS_NewURL( &uri, aURL );
#else  // NECKO
    rv = NS_NewURI( &uri, aURL );
#endif // NECKO

    if ( NS_SUCCEEDED( rv ) && uri ) {
        rv = this->SelectFileAndTransferLocation( uri,parent );
        NS_RELEASE( uri );
    }

    return rv;
}

NS_IMETHODIMP
nsStreamTransfer::SelectFile( nsFileSpec &aResult ) {
    nsresult rv = NS_OK;

    // Prompt user for file name.
    nsIFileWidget* fileWidget;
  
    nsString title("Save File");

    rv = nsComponentManager::CreateInstance( kCFileWidgetCID,
                                             nsnull,
                                             kIFileWidgetIID,
                                             (void**)&fileWidget );
    
    if ( NS_SUCCEEDED( rv ) && fileWidget ) {
        nsFileDlgResults result = fileWidget->PutFile( nsnull, title, aResult );
        if ( result == nsFileDlgResults_OK || result == nsFileDlgResults_Replace ) {
        } else {
            rv = NS_ERROR_ABORT;
        }
        NS_RELEASE( fileWidget );
    } else {
        DEBUG_PRINTF( PR_STDOUT, "%s %d: Error creating file widget, rv=0x%X\n",
                      __FILE__, (int)__LINE__, (int)rv );
    }
    return rv;
}

// Generate base nsIAppShellComponent implementation.
NS_IMPL_IAPPSHELLCOMPONENT( nsStreamTransfer,
                            nsIStreamTransfer,
                            NS_ISTREAMTRANSFER_PROGID,
                            0 )
