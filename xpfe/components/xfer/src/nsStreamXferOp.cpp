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
#include "nsStreamXferOp.h"

#include "nsIStreamTransfer.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsFileStream.h"
#include "nsFileSpec.h"
#include "nsIObserver.h"
#include "nsIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#ifndef NECKO
#include "nsIServiceManager.h"
#include "nsINetService.h"
static NS_DEFINE_IID( kNetServiceCID,      NS_NETSERVICE_CID );
#else
#include "nsNeckoUtil.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsIEventQueueService.h"
#include "nsIBufferInputStream.h"
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
#endif // NECKO
#include "jsapi.h"
#include "prprf.h"

#ifdef NS_DEBUG
#define DEBUG_PRINTF PR_fprintf
#else
#define DEBUG_PRINTF (void)
#endif

// ctor - save arguments in data members.
nsStreamXferOp::nsStreamXferOp( const nsString &source, const nsString &target ) 
    : mSource( source ),
      mTarget( target ),
      mObserver( 0 ),
      mBufLen( 8192 ),
      mBuffer( new char[ mBufLen ] ),
      mStopped( PR_FALSE ),
      mOutput( 0 ) {
    // Properly initialize refcnt.
    NS_INIT_REFCNT();
}

// dtor
nsStreamXferOp::~nsStreamXferOp() {
    // Delete dynamically allocated members (file and buffer).
    DEBUG_PRINTF( PR_STDOUT, "nsStreamXferOp destructor called\n" );
    delete mOutput;
    delete [] mBuffer;
}

// Invoke nsIDOMWindow::OpenDialog, passing this object as argument.
NS_IMETHODIMP
nsStreamXferOp::OpenDialog( nsIDOMWindow *parent ) {
    nsresult rv = NS_OK;
    // Get JS context from parent window.
    nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface( parent, &rv );
    if ( NS_SUCCEEDED( rv ) && sgo ) {
        nsCOMPtr<nsIScriptContext> context;
        sgo->GetContext( getter_AddRefs( context ) );
        if ( context ) {
            JSContext *jsContext = (JSContext*)context->GetNativeContext();
            if ( jsContext ) {
                void *stackPtr;
                jsval *argv = JS_PushArguments( jsContext,
                                                &stackPtr,
                                                "svs%ip",
                                                "chrome://global/content/downloadProgress.xul",
                                                JSVAL_NULL,
                                                "chrome",
                                                (const nsIID*)(&nsCOMTypeInfo<nsIStreamTransferOperation>::GetIID()),
                                                (nsISupports*)(nsIStreamTransferOperation*)this );
                if ( argv ) {
                    nsIDOMWindow *newWindow;
                    rv = parent->OpenDialog( jsContext, argv, 4, &newWindow );
                    if ( NS_SUCCEEDED( rv ) ) {
                        newWindow->Release();
                    } else {
                    }
                    JS_PopArguments( jsContext, stackPtr );
                } else {
                    DEBUG_PRINTF( PR_STDOUT, "%s %d: JS_PushArguments failed\n",
                                  (char*)__FILE__, (int)__LINE__ );
                    rv = NS_ERROR_FAILURE;
                }
            } else {
                DEBUG_PRINTF( PR_STDOUT, "%s %d: GetNativeContext failed\n",
                              (char*)__FILE__, (int)__LINE__ );
                rv = NS_ERROR_FAILURE;
            }
        } else {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: GetContext failed\n",
                          (char*)__FILE__, (int)__LINE__ );
            rv = NS_ERROR_FAILURE;
        }
    } else {
        DEBUG_PRINTF( PR_STDOUT, "%s %d: QueryInterface (for nsIScriptGlobalObject) failed, rv=0x%08X\n",
                      (char*)__FILE__, (int)__LINE__, (int)rv );
    }
    return rv;
}

// Start the download by opening the output file and then loading the source location.
NS_IMETHODIMP
nsStreamXferOp::Start( void ) {
    nsresult rv = NS_OK;

    if ( !mOutput ) {
        // Open the output file stream.
        mOutput = new nsOutputFileStream( nsFileSpec( mTarget.GetBuffer() ) );
        if ( mOutput ) {
            if ( !mOutput->failed() ) {
                nsIURI *url = 0;
#ifndef NECKO
                rv = NS_NewURL( &url, mSource.GetBuffer() );
                if ( NS_SUCCEEDED( rv ) && url ) {
                    nsINetService *inet = 0;
                    nsresult rv = nsServiceManager::GetService( kNetServiceCID,
                                                                nsCOMTypeInfo<nsINetService>::GetIID(),
                                                                (nsISupports**)&inet );
                    if ( NS_SUCCEEDED( rv ) && inet ) {
                        rv = inet->OpenStream( url, this );
                        if ( NS_FAILED( rv ) ) {
                            DEBUG_PRINTF( PR_STDOUT, "%s %d: OpenStream failed, rv=0x%08X\n",
                                          (char*)__FILE__, (int)__LINE__, (int)rv );
                        }
                        nsServiceManager::ReleaseService( kNetServiceCID, inet );
                    } else {
                        DEBUG_PRINTF( PR_STDOUT, "%s %d: Error getting Net Service, rv=0x%X\n",
                                      __FILE__, (int)__LINE__, (int)rv );
                    }
                } else {
                    DEBUG_PRINTF( PR_STDOUT, "%s %d: NS_NewURL failed, rv=0x%X\n",
                                  __FILE__, (int)__LINE__, (int)rv );
                }
#else
                rv = NS_NewURI( &url, mSource.GetBuffer() );
                if ( NS_SUCCEEDED( rv ) && url ) {
                    nsresult rv = NS_OpenURI( this, nsnull, url );
                
                    if ( NS_FAILED( rv ) ) {
                        DEBUG_PRINTF( PR_STDOUT, "%s %d: NS_OpenURI failed, rv=0x%08X\n",
                                      (char*)__FILE__, (int)__LINE__, (int)rv );
                    }
                } else {
                    DEBUG_PRINTF( PR_STDOUT, "%s %d: NS_NewURI failed, rv=0x%X\n",
                                  __FILE__, (int)__LINE__, (int)rv );
                }
#endif // NECKO
        
            } else {
                DEBUG_PRINTF( PR_STDOUT, "%s %d: error opening output file, rv=0x%08X\n",
                              (char*)__FILE__, (int)__LINE__, (int)mOutput->error() );
                delete mOutput;
                mOutput = 0;
            }
        } else {
            rv = NS_ERROR_OUT_OF_MEMORY;
        }

    } else {
        DEBUG_PRINTF( PR_STDOUT, "%s %d: nsStreamXferOp already started\n",
                      (char*)__FILE__, (int)__LINE__ );
        rv = NS_ERROR_ALREADY_INITIALIZED;
    }

    return rv;
}

// Terminate the download by setting flag (checked in OnDataAvailable).
NS_IMETHODIMP
nsStreamXferOp::Stop( void ) {
    nsresult rv = NS_OK;

    // Set flag indicating netlib xfer should cease.
    mStopped = PR_TRUE;

    return rv;
}

// Process the data by reading it and then writing it to the output file.
NS_IMETHODIMP
#ifdef NECKO
nsStreamXferOp::OnDataAvailable( nsIChannel     *channel,
                                 nsISupports    *aContext,
                                 nsIInputStream *aIStream,
                                 PRUint32        offset,
                                 PRUint32        aLength ) {
#else
nsStreamXferOp::OnDataAvailable( nsIURI         *aURL,
                                 nsIInputStream *aIStream,
                                 PRUint32        aLength ) {
#endif
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
                if ( mOutput->failed() ) {
                    DEBUG_PRINTF( PR_STDOUT, "%s %d: Error writing file, rv=0x%08X\n",
                                  (char*)__FILE__, (int)__LINE__, (int)mOutput->error() );
                }
            }
        } else {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: Error reading stream, rv=0x%08X\n",
                          (char*)__FILE__, (int)__LINE__, (int)rv );
        }
    }

    return rv;
}

// We aren't interested in this notification; simply return NS_OK.
NS_IMETHODIMP
#ifdef NECKO
nsStreamXferOp::OnStartRequest(nsIChannel* channel, nsISupports* aContext) {
#else
nsStreamXferOp::OnStartRequest(nsIURI* aURL, const char *aContentType) {
#endif
    nsresult rv = NS_OK;

    return rv;
}

// Pass notification to our observer (if we have one). This object is the
// "subject", the topic is the component progid (plus ";onProgress"), and
// the data is the progress numbers (in the form "%lu %lu" where the first
// value is the number of bytes processed, the second the total number
// expected.
NS_IMETHODIMP
#ifdef NECKO
nsStreamXferOp::OnProgress(nsIChannel* channel, nsISupports* aContext,
                                     PRUint32 aProgress, PRUint32 aProgressMax) {
#else
nsStreamXferOp::OnProgress(nsIURI* aURL, PRUint32 aProgress, PRUint32 aProgressMax) {
#endif
    nsresult rv = NS_OK;

    if ( mObserver ) {
        char buf[32];
        PR_snprintf( buf, sizeof buf, "%lu %lu", (unsigned long)aProgress, (unsigned long)aProgressMax );
        rv = mObserver->Observe( (nsIStreamTransferOperation*)this,
                                  nsString( NS_ISTREAMTRANSFER_PROGID ";onProgress" ).GetUnicode(),
                                  nsString( buf ).GetUnicode() );
        if ( NS_FAILED( rv ) ) {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: Observe failed, rv=0x%08X\n",
                          (char*)__FILE__, (int)__LINE__, (int)rv );
        }
    }

    return rv;
}

// Pass notification to our observer (if we have one). This object is the
// "subject", the topic is the component progid (plus ";onStatus"), and
// the data is the status text.
NS_IMETHODIMP
#ifdef NECKO
nsStreamXferOp::OnStatus( nsIChannel      *channel,
                          nsISupports     *aContext,
                          const PRUnichar *aMsg ) {
#else
nsStreamXferOp::OnStatus( nsIURI          *aURL,
                          const PRUnichar *aMsg ) {
#endif
    nsresult rv = NS_OK;

    if ( mObserver ) {
        nsString msg = aMsg;
        rv = mObserver->Observe( (nsIStreamTransferOperation*)this,
                                  nsString( NS_ISTREAMTRANSFER_PROGID ";onStatus" ).GetUnicode(),
                                  nsString( msg ).GetUnicode() );
        if ( NS_FAILED( rv ) ) {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: Observe failed, rv=0x%08X\n",
                          (char*)__FILE__, (int)__LINE__, (int)rv );
        }
    }

    return rv;
}

// Close the output stream. In addition, notify our observer
// (if we have one).
NS_IMETHODIMP
#ifdef NECKO
nsStreamXferOp::OnStopRequest( nsIChannel      *channel,
                               nsISupports     *aContext,
                               nsresult         aStatus,
                               const PRUnichar *aMsg ) {
#else
nsStreamXferOp::OnStopRequest( nsIURI          *aURL,
                               nsresult         aStatus,
                               const PRUnichar *aMsg ) {
#endif
    nsresult rv = NS_OK;

    // Close the output file.
    if ( mOutput ) {
        mOutput->close();
    }

    // Notify observer.
    if ( mObserver ) {
        nsString msg = aMsg;
        rv = mObserver->Observe( (nsIStreamTransferOperation*)this,
                                  nsString( NS_ISTREAMTRANSFER_PROGID ";onCompletion" ).GetUnicode(),
                                  nsString( msg ).GetUnicode() );
        if ( NS_FAILED( rv ) ) {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: Observe failed, rv=0x%08X\n",
                          (char*)__FILE__, (int)__LINE__, (int)rv );
        }
    }

    return rv;
}

// Attribute getters/setters...

NS_IMETHODIMP
nsStreamXferOp::GetSource( char**result ) {
    nsresult rv = NS_OK;

    if ( result ) {
        *result = mSource.ToNewCString();
        if ( !*result ) {
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}

NS_IMETHODIMP
nsStreamXferOp::GetTarget( char**result ) {
    nsresult rv = NS_OK;

    if ( result ) {
        *result = mTarget.ToNewCString();
        if ( !*result ) {
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}

NS_IMETHODIMP
nsStreamXferOp::GetObserver( nsIObserver**result ) {
    nsresult rv = NS_OK;

    if ( result ) {
        *result = mObserver;
        NS_IF_ADDREF( *result );
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}

NS_IMETHODIMP
nsStreamXferOp::SetObserver( nsIObserver*aObserver ) {
    nsresult rv = NS_OK;

    NS_IF_RELEASE( mObserver );
    mObserver = aObserver;
    NS_IF_ADDREF( mObserver );

    return rv;
}

// Generate standard implementation of AddRef/Release for nsStreamXferOp
NS_IMPL_ADDREF( nsStreamXferOp )
NS_IMPL_RELEASE( nsStreamXferOp )

// QueryInterface, supports all the interfaces we implement.
NS_IMETHODIMP 
nsStreamXferOp::QueryInterface( REFNSIID aIID, void** aInstancePtr ) {
    if (aInstancePtr == NULL) {
        return NS_ERROR_NULL_POINTER;
    }
    
    // Always NULL result, in case of failure
    *aInstancePtr = NULL;
    
    #ifdef NECKO
    if (aIID.Equals(nsCOMTypeInfo<nsIProgressEventSink>::GetIID())) {
        *aInstancePtr = (void*) ((nsIProgressEventSink*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    #endif
    if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
        *aInstancePtr = (void*) ((nsIStreamObserver*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsCOMTypeInfo<nsIStreamObserver>::GetIID())) {
        *aInstancePtr = (void*) ((nsIStreamObserver*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsCOMTypeInfo<nsIStreamListener>::GetIID())) {
        *aInstancePtr = (void*) ((nsIStreamListener*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsCOMTypeInfo<nsIStreamTransferOperation>::GetIID())) {
        *aInstancePtr = (void*) ((nsIStreamTransferOperation*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    
    return NS_ERROR_NO_INTERFACE;
}
