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
#include "nsStreamXferOp.h"
#include "nsIStreamTransfer.h"

// Basic dependencies.
#include "nsIServiceManager.h"
#include "nsISupportsPrimitives.h"

// For notifying observer.
#include "nsIObserver.h"

// For opening dialog.
#include "nsIDOMWindowInternal.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPrompt.h"
#include "jsapi.h"

// For opening input/output streams.
#include "nsIFileTransportService.h"
#include "nsIOutputStream.h"
#include "nsNetUtil.h"
#include "nsILocalFile.h"
#include "nsICachingChannel.h"

#include "prprf.h"

#include "nsIStringBundle.h"
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#ifdef NS_DEBUG
#define DEBUG_PRINTF PR_fprintf
#else
#define DEBUG_PRINTF (void)
#endif

#ifdef USE_ASYNC_READ
NS_IMPL_ISUPPORTS5(nsStreamXferOp, nsIStreamListener, nsIRequestObserver, nsIStreamTransferOperation, nsIProgressEventSink, nsIInterfaceRequestor);
#else
NS_IMPL_ISUPPORTS4(nsStreamXferOp, nsIRequestObserver, nsIStreamTransferOperation, nsIProgressEventSink, nsIInterfaceRequestor);
#endif

// ctor - save arguments in data members.
nsStreamXferOp::nsStreamXferOp( nsIChannel *source, nsILocalFile *target ) 
    : mInputChannel( source ),
      mOutputTransport( 0 ),
      mOutputStream( 0 ),
      mOutputFile( target ),
      mObserver( 0 ),
      mContentLength( 0 ),
      mBytesProcessed( 0 ),
      mError( PR_FALSE ) {
    // Properly initialize refcnt.
    NS_INIT_REFCNT();
}

// dtor
nsStreamXferOp::~nsStreamXferOp() {
    // Delete dynamically allocated members (file and buffer).
}

// Invoke nsIDOMWindowInternal::OpenDialog, passing this object as argument.
NS_IMETHODIMP
nsStreamXferOp::OpenDialog( nsIDOMWindowInternal *parent ) {
    nsresult rv = NS_OK;

    nsCOMPtr<nsISupportsInterfacePointer> ifptr =
        do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    ifptr->SetData(NS_STATIC_CAST(nsIStreamTransferOperation *, this));
    ifptr->SetDataIID(&NS_GET_IID(nsIStreamTransferOperation));

    mParentWindow = parent;

    // Open the dialog.
    nsCOMPtr<nsIDOMWindow> newWindow;

    rv = parent->OpenDialog(NS_LITERAL_STRING("chrome://global/content/downloadProgress.xul"),
                            NS_LITERAL_STRING("_blank"),
                            NS_LITERAL_STRING("chrome,titlebar,minimizable"),
                            ifptr, getter_AddRefs(newWindow));

    return rv;
}

// Notify observer of error.
NS_IMETHODIMP
nsStreamXferOp::OnError( int operation, nsresult errorCode ) {
    nsresult rv = NS_OK;

    // Record fact that an error has occurred.
    mError = PR_TRUE;

    if ( mObserver ) {
        // Pick off error codes of interest and convert to strings
        // so JS code can test for them without hardcoded numbers.
        PRUint32 reason = 0;
        if ( errorCode == NS_ERROR_FILE_ACCESS_DENIED ) {
            reason = kReasonAccessError;
        } else if ( errorCode == NS_ERROR_FILE_READ_ONLY ) {
            reason = kReasonAccessError;
        } else if ( errorCode == NS_ERROR_FILE_NO_DEVICE_SPACE ) {
            reason = kReasonDiskFull;
        } else if ( errorCode == NS_ERROR_FILE_DISK_FULL ) {
            reason = kReasonDiskFull;
        }
        char buf[64];
        PR_snprintf( buf, sizeof( buf ), "%d %X %u", operation, (int)errorCode, reason );
        rv = mObserver->Observe( (nsIStreamTransferOperation*)this,
                                 NS_ISTREAMTRANSFER_CONTRACTID ";onError",
                                 NS_ConvertASCIItoUCS2( buf ).get() );
        if ( NS_FAILED( rv ) ) {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: Observe failed, rv=0x%08X\n",
                          (char*)__FILE__, (int)__LINE__, (int)rv );
        }
    }

    return rv;
};

// Start the download by opening the output file and then reading the input channel.
NS_IMETHODIMP
nsStreamXferOp::Start( void ) {
    nsresult rv = NS_OK;

    if ( mInputChannel ) {
        if ( !mOutputTransport ) {
            // First, get file transport service.
            NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
            nsCOMPtr<nsIFileTransportService> fts = 
                     do_GetService( kFileTransportServiceCID, &rv );
    
            if ( NS_SUCCEEDED( rv ) ) {
                // Next, create output file channel.

                rv = fts->CreateTransport( mOutputFile, 
                                           PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                                           0664,
                                           getter_AddRefs( mOutputTransport ) );
    
                if ( NS_SUCCEEDED( rv ) ) {
#ifdef USE_ASYNC_READ
                    // Read the input channel (with ourself as the listener).
                    rv = mInputChannel->AsyncOpen(this, nsnull);
                    if ( NS_FAILED( rv ) ) {
                        this->OnError( kOpAsyncRead, rv );
                    }
#else // USE_ASYNC_READ
                    // reset the channel's interface requestor so we receive status
                    // notifications.
                    rv = mInputChannel->SetNotificationCallbacks(NS_STATIC_CAST(nsIInterfaceRequestor*,this));
                    if (NS_FAILED(rv)) {
                        this->OnError(0, rv);
                        return rv;
                    }

                    // get the input stream from the channel.
                    nsCOMPtr<nsIInputStream> inStream;
                    rv = mInputChannel->Open(getter_AddRefs(inStream));
                    if (NS_FAILED(rv)) {
                        this->OnError(0, rv);
                        return rv;
                    }

                    // hand the output channel our input stream. it will take care
                    // of reading data from the stream and writing it to disk.
                    nsCOMPtr<nsIRequest> dummyRequest;
                    rv = NS_AsyncWriteFromStream(getter_AddRefs(dummyRequest),
                                                 mOutputTransport, inStream, 0, -1, 0,
                            NS_STATIC_CAST(nsIRequestObserver*, this));
                    if ( NS_FAILED( rv ) ) {
                        this->OnError( kOpAsyncWrite, rv );
                    }
#endif // USE_ASYNC_READ
                } else {
                    this->OnError( kOpCreateTransport, rv );
                    rv = NS_ERROR_OUT_OF_MEMORY;
                }
            } else {
                this->OnError( kOpGetService, rv );
            }
        } else {
            rv = NS_ERROR_ALREADY_INITIALIZED;
            this->OnError( 0, rv );
        }
    } else {
        rv = NS_ERROR_NOT_INITIALIZED;
        this->OnError( 0, rv );
    }

    // If an error occurred, shut down.
    if ( NS_FAILED( rv ) ) {
        this->Stop();
    }

    return rv;
}

// Terminate the download by cancelling/closing input and output channels.
NS_IMETHODIMP
nsStreamXferOp::Stop( void ) {
    nsresult rv = NS_OK;

    // Cancel input.
    if ( mInputChannel ) {
        // Unhook it first.
        nsCOMPtr<nsIChannel> channel = mInputChannel;
        mInputChannel = 0;
        // Now cancel it.
        if (channel)
            rv = channel->Cancel(NS_BINDING_ABORTED);
        if ( NS_FAILED( rv ) ) {
            this->OnError( kOpInputCancel, rv );
        }
    }

    // Close output stream.
    if ( mOutputStream ) {
        // Unhook it first.
        nsCOMPtr<nsIOutputStream> stream = mOutputStream;
        mOutputStream = 0;

        // Now close it.
        rv = stream->Close();
        if ( NS_FAILED( rv ) ) {
            this->OnError( kOpOutputClose, rv );
        }
    }

    // Cancel output channel.
    mOutputTransport = 0;

    return rv;
}

// This is called when the input channel is successfully opened.
//
// We also open the output stream at this point.
NS_IMETHODIMP
nsStreamXferOp::OnStartRequest(nsIRequest *request, nsISupports* aContext) {
    nsresult rv = NS_OK;

#ifdef DEBUG_law
    DEBUG_PRINTF( PR_STDOUT, "nsStreamXferOp::OnStartRequest; request=0x%08X, context=0x%08X\n",
                  (int)(void*)request, (int)(void*)aContext );
#endif

#ifdef USE_ASYNC_READ
    if ( !mOutputStream && mOutputTransport ) {
        // Open output stream.
        rv = mOutputTransport->OpenOutputStream(0,-1,0,getter_AddRefs( mOutputStream ) );

        if ( NS_FAILED( rv ) ) {
            // Give up all hope.
            this->OnError( kOpOpenOutputStream, rv );
            this->Stop();
        }
    }
#endif // USE_ASYNC_READ

    return rv;
}

#ifdef USE_ASYNC_READ
// Process the data by writing it to the output channel.
NS_IMETHODIMP
nsStreamXferOp::OnDataAvailable( nsIRequest*    request,
                                 nsISupports    *aContext,
                                 nsIInputStream *aIStream,
                                 PRUint32        offset,
                                 PRUint32        aLength ) {
    nsresult rv = NS_OK;

    if ( mOutputStream ) {
        // Write the data to the output stream.
        // Read a buffer full till aLength bytes have been processed.
        char buffer[ 8192 ];
        char *bufPtr = buffer;
        unsigned long bytesToRead = aLength;
        while ( NS_SUCCEEDED( rv ) && bytesToRead ) {
            // Write out remainder of read buffer.
            unsigned int bytesToWrite;
            // Read a buffer full or the number remaining (whichever is smaller).
            rv = aIStream->Read( buffer,
                                 PR_MIN( sizeof( buffer ), bytesToRead ),
                                 &bytesToWrite );
            if ( NS_SUCCEEDED( rv ) ) {
                // Record bytes that have been read so far.
                bytesToRead -= bytesToWrite;

                // Write out the data until something goes wrong, or, it is
                // all written.  We loop because for some errors (e.g., disk
                // full), we get NS_OK with some bytes written, then an error.
                // So, we want to write again in that case to get the actual
                // error code.
                const char *bufPtr = buffer; // Where to write from.
                while ( NS_SUCCEEDED( rv ) && bytesToWrite ) {
                    unsigned int bytesWritten = 0;
                    rv = mOutputStream->Write( buffer, bytesToWrite, &bytesWritten );
                    if ( NS_SUCCEEDED( rv ) ) {
                        // Adjust count of amount to write and the write location.
                        bytesToWrite -= bytesWritten;
                        bufPtr       += bytesWritten;
                        // Force an error if (for some reason) we get NS_OK but
                        // no bytes written.
                        if ( !bytesWritten ) {
                            rv = NS_ERROR_FAILURE;
                            this->OnError( kOpWrite, rv );
                        }
                    } else {
                        // Something is wrong.
                        this->OnError( kOpWrite, rv );
                    }
                }
            } else {
                this->OnError( kOpRead, rv );
            }
        }
    } else {
        rv = NS_ERROR_NOT_INITIALIZED;
        this->OnError( 0, rv );
    }

    if ( NS_FAILED( rv ) ) {
        // Oh dear.  close up shop.
        this->Stop();
    } else {
        // Fake OnProgress.
        mBytesProcessed += aLength;
        if ( mContentLength == 0 && request ) {
            // Get content length from input channel.
            nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
            if (!channel) return NS_ERROR_FAILURE;
            channel->GetContentLength( &mContentLength );
        }
        this->OnProgress( request, 0, mBytesProcessed, mContentLength );
    }

    return rv;
}
#endif // USE_ASYNC_READ

// As an event sink getter, we get ourself.
NS_IMETHODIMP
nsStreamXferOp::GetInterface(const nsIID &anIID, void **aResult ) {
    return this->QueryInterface( anIID, (void**)aResult );
}

// Pass notification to our observer (if we have one). This object is the
// "subject", the topic is the component contractid (plus ";onProgress"), and
// the data is the progress numbers (in the form "%lu %lu" where the first
// value is the number of bytes processed, the second the total number
// expected.
//
NS_IMETHODIMP
nsStreamXferOp::OnProgress(nsIRequest *request, nsISupports* aContext,
                                     PRUint32 aProgress, PRUint32 aProgressMax) {
    nsresult rv = NS_OK;

    if (mContentLength < 1) {
        NS_ASSERTION(request, "should have a request");
        nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
        if (!channel) return NS_ERROR_FAILURE;
        rv = channel->GetContentLength(&mContentLength);
        if (NS_FAILED(rv)) return rv;
    }

    if ( mObserver ) {
        char buf[32];
        PR_snprintf( buf, sizeof buf, "%lu %ld", (unsigned long)aProgress, (long)mContentLength );
        rv = mObserver->Observe( (nsIStreamTransferOperation*)this,
                                  NS_ISTREAMTRANSFER_CONTRACTID ";onProgress",
                                  NS_ConvertASCIItoUCS2( buf ).get() );
        if ( NS_FAILED( rv ) ) {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: Observe failed, rv=0x%08X\n",
                          (char*)__FILE__, (int)__LINE__, (int)rv );
        }
    }

    return rv;
}

// Pass notification to our observer (if we have one). This object is the
// "subject", the topic is the component contractid (plus ";onStatus"), and
// the data is the status text.
NS_IMETHODIMP
nsStreamXferOp::OnStatus( nsIRequest      *request,
                          nsISupports     *aContext,
                          nsresult        aStatus,
                          const PRUnichar *aStatusArg) {
    nsresult rv = NS_OK;

    if ( mObserver ) {
        nsCOMPtr<nsIStringBundleService> sbs = do_GetService(kStringBundleServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;
        nsXPIDLString str;
        rv = sbs->FormatStatusMessage(aStatus, aStatusArg, getter_Copies(str));
        if (NS_FAILED(rv)) return rv;
        nsAutoString msg(NS_STATIC_CAST(const PRUnichar*, str));
        rv = mObserver->Observe( (nsIStreamTransferOperation*)this,
                                  NS_ISTREAMTRANSFER_CONTRACTID ";onStatus",
                                  msg.get() );
        if ( NS_FAILED( rv ) ) {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: Observe failed, rv=0x%08X\n",
                          (char*)__FILE__, (int)__LINE__, (int)rv );
        }
    }

    return rv;
}

#define DIALOG_STRING_URI "chrome://global/locale/appstrings.properties"

// This is called when the end of input is reached on the input channel.
NS_IMETHODIMP
nsStreamXferOp::OnStopRequest( nsIRequest      *request,
                               nsISupports     *aContext,
                               nsresult         aStatus) {
    nsresult rv = NS_OK;
    
#ifdef USE_ASYNC_READ
    // A document that was requested to be fetched *only* from
    // the cache is not in cache. The cache only restriction probably
    // existed because we have post data. Confirm with the user
    // that we want to do a repost.
    if (aStatus == NS_ERROR_DOCUMENT_NOT_CACHED) {
        PRBool repost;
        nsCOMPtr<nsIStringBundle> stringBundle;
        
        nsCOMPtr<nsIStringBundleService> sbs(do_GetService(kStringBundleServiceCID, &rv));
        if (sbs) {
            sbs->CreateBundle(DIALOG_STRING_URI,
                              getter_AddRefs(stringBundle));
        }
        
        if (stringBundle) {
            nsXPIDLString messageStr;
            nsresult rv = stringBundle->GetStringFromName(NS_LITERAL_STRING("repost").get(), 
                                                          getter_Copies(messageStr));
            
            if (NS_SUCCEEDED(rv) && messageStr && mParentWindow) {
                mParentWindow->Confirm(messageStr, &repost);
                // If the user pressed cancel in the dialog, don't try 
                // to load the page with out the post data and bail out
                // with a success code.
                if (!repost) {
                    aStatus = NS_OK;
                }
                // Otherwise just start over again with the same
                // channel.
                else {
                    nsCOMPtr<nsICachingChannel> cachingChannel = do_QueryInterface(mInputChannel);
                    // null out the cache key and don't require a 
                    // cached load
                    if (cachingChannel) {
                        cachingChannel->SetCacheKey(nsnull, PR_FALSE);
                    }
                    
                    // Reopen the channel and hope it works.
                    rv = mInputChannel->AsyncOpen(this, nsnull);
                    if (NS_SUCCEEDED(rv)) {
                        return NS_OK;
                    }
                }
            }
        }
    }
#endif // USE_ASYNC_READ

    // If an error occurred, shut down.
    if ( NS_FAILED( aStatus ) ) {
        this->Stop();
        this->OnError( kOpAsyncRead, aStatus );
    }

    // Close the output stream.
    if ( mOutputStream ) {
        rv = mOutputStream->Close();
        if ( NS_FAILED( rv ) ) {
            this->OnError( kOpOutputClose, rv );
        }
    }

#ifdef XP_MAC
    // Mac only: set the file type and creator based on Internet Config settings
    
    if (mInputChannel)
    {
        // Get the content type
        nsXPIDLCString contentType;
        rv = mInputChannel->GetContentType(getter_Copies(contentType));
        
        if (NS_SUCCEEDED(rv))
        {
            // Set the creator and file type on the output file
            nsCOMPtr<nsILocalFileMac> macFile = do_QueryInterface(mOutputFile);
            if (contentType.get() && *contentType.get() && macFile)
                macFile->SetFileTypeAndCreatorFromMIMEType(contentType.get());
        }
    }
#endif // XP_MAC
    
    // Unhook input/output channels (don't need to cancel 'em).
    mInputChannel = 0;
    mOutputTransport = 0;

    // Notify observer that the download is complete.
    if ( !mError && mObserver ) {
        nsCOMPtr<nsIObserver> kungFuDeathGrip(mObserver);
        rv = kungFuDeathGrip->Observe( (nsIStreamTransferOperation*)this,
                                       NS_ISTREAMTRANSFER_CONTRACTID ";onCompletion",
                                       nsnull );
        if ( NS_FAILED( rv ) ) {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: Observe failed, rv=0x%08X\n",
                          (char*)__FILE__, (int)__LINE__, (int)rv );
        }
    }

    return rv;
}

// Attribute getters/setters...

NS_IMETHODIMP
nsStreamXferOp::GetSource( nsIChannel**result ) {
    nsresult rv = NS_OK;

    if ( result ) {
        *result = mInputChannel;
        NS_IF_ADDREF( *result );
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}

NS_IMETHODIMP
nsStreamXferOp::GetTarget( nsILocalFile**result ) {
    nsresult rv = NS_OK;

    if ( result ) {
        *result = mOutputFile;
        NS_IF_ADDREF( *result );
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
