/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#include "nsStreamXferOp.h"
#include "nsIStreamTransfer.h"

// Basic dependencies.
#include "nsIServiceManager.h"

// For notifying observer.
#include "nsIObserver.h"

// For opening dialog.
#include "nsIDOMWindowInternal.h"
#include "nsIScriptGlobalObject.h"
#include "jsapi.h"

// For opening input/output streams.
#include "nsIFileTransportService.h"
#include "nsIOutputStream.h"
#include "nsNetUtil.h"
#include "nsILocalFile.h"

#include "prprf.h"

#include "nsIStringBundle.h"
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#ifdef NS_DEBUG
#define DEBUG_PRINTF PR_fprintf
#else
#define DEBUG_PRINTF (void)
#endif

#ifdef USE_ASYNC_READ
NS_IMPL_ISUPPORTS5(nsStreamXferOp, nsIStreamListener, nsIStreamObserver, nsIStreamTransferOperation, nsIProgressEventSink, nsIInterfaceRequestor);
#else
NS_IMPL_ISUPPORTS4(nsStreamXferOp, nsIStreamObserver, nsIStreamTransferOperation, nsIProgressEventSink, nsIInterfaceRequestor);
#endif

// ctor - save arguments in data members.
nsStreamXferOp::nsStreamXferOp( nsIChannel *source, nsILocalFile *target ) 
    : mInputChannel( source ),
      mOutputChannel( 0 ),
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
                                                "sss%ip",
                                                "chrome://global/content/downloadProgress.xul",
                                                "_blank",
                                                "chrome,titlebar",
                                                (const nsIID*)(&NS_GET_IID(nsIStreamTransferOperation)),
                                                (nsISupports*)(nsIStreamTransferOperation*)this );
                if ( argv ) {
                    nsCOMPtr<nsIDOMWindowInternal> newWindow;
                    rv = parent->OpenDialog( jsContext, argv, 4, getter_AddRefs( newWindow ) );
                    if ( NS_FAILED( rv ) ) {
                        DEBUG_PRINTF( PR_STDOUT, "%s %d: nsIDOMWindowInternal::OpenDialog failed, rv=0x%08X\n",
                                      (char*)__FILE__, (int)__LINE__, (int)rv );
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
                                 NS_ConvertASCIItoUCS2( NS_ISTREAMTRANSFER_CONTRACTID ";onError" ).GetUnicode(),
                                 NS_ConvertASCIItoUCS2( buf ).GetUnicode() );
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
        if ( !mOutputChannel ) {
            // First, get file transport service.
            NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
            NS_WITH_SERVICE( nsIFileTransportService, fts, kFileTransportServiceCID, &rv );
    
            if ( NS_SUCCEEDED( rv ) ) {
                // Next, create output file channel.
                rv = fts->CreateTransport( mOutputFile, 
                                           PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                                           0664,
                                           getter_AddRefs( mOutputChannel ) );
    
                if ( NS_SUCCEEDED( rv ) ) {
#ifdef USE_ASYNC_READ
                    // Read the input channel (with ourself as the listener).
                    rv = mInputChannel->AsyncRead(this, nsnull);
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
                    rv = mInputChannel->OpenInputStream(getter_AddRefs(inStream));
                    if (NS_FAILED(rv)) {
                        this->OnError(0, rv);
                        return rv;
                    }

                    // hand the output channel our input stream. it will take care
                    // of reading data from the stream and writing it to disk.
                    rv = NS_AsyncWriteFromStream(mOutputChannel, inStream, NS_STATIC_CAST(nsIStreamObserver*,this), nsnull);
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
    mOutputChannel = 0;

    return rv;
}

// This is called when the input channel is successfully opened.
//
// We also open the output stream at this point.
NS_IMETHODIMP
nsStreamXferOp::OnStartRequest(nsIChannel* channel, nsISupports* aContext) {
    nsresult rv = NS_ERROR_FAILURE;

#ifdef USE_ASYNC_READ
    if ( !mOutputStream ) {
        // Open output stream.
        rv = mOutputChannel->OpenOutputStream( getter_AddRefs( mOutputStream ) );
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
nsStreamXferOp::OnDataAvailable( nsIChannel     *channel,
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
        if ( mContentLength == 0 && channel ) {
            // Get content length from input channel.
            channel->GetContentLength( &mContentLength );
        }
        this->OnProgress( mOutputChannel, 0, mBytesProcessed, mContentLength );
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
nsStreamXferOp::OnProgress(nsIChannel* channel, nsISupports* aContext,
                                     PRUint32 aProgress, PRUint32 aProgressMax) {
    nsresult rv = NS_OK;

    if (mContentLength < 1) {
        NS_ASSERTION(channel, "should have a channel");
        rv = channel->GetContentLength(&mContentLength);
        if (NS_FAILED(rv)) return rv;
    }

    if ( mObserver ) {
        char buf[32];
        PR_snprintf( buf, sizeof buf, "%lu %ld", (unsigned long)aProgress, (long)mContentLength );
        rv = mObserver->Observe( (nsIStreamTransferOperation*)this,
                                  NS_ConvertASCIItoUCS2( NS_ISTREAMTRANSFER_CONTRACTID ";onProgress" ).GetUnicode(),
                                  NS_ConvertASCIItoUCS2( buf ).GetUnicode() );
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
nsStreamXferOp::OnStatus( nsIChannel      *channel,
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
                                  NS_ConvertASCIItoUCS2( NS_ISTREAMTRANSFER_CONTRACTID ";onStatus" ).GetUnicode(),
                                  msg.GetUnicode() );
        if ( NS_FAILED( rv ) ) {
            DEBUG_PRINTF( PR_STDOUT, "%s %d: Observe failed, rv=0x%08X\n",
                          (char*)__FILE__, (int)__LINE__, (int)rv );
        }
    }

    return rv;
}

// This is called when the end of input is reached on the input channel.
NS_IMETHODIMP
nsStreamXferOp::OnStopRequest( nsIChannel      *channel,
                               nsISupports     *aContext,
                               nsresult         aStatus,
                               const PRUnichar *aMsg ) {
    nsresult rv = NS_OK;

    // If an error occurred, shut down.
    if ( NS_FAILED( aStatus ) ) {
        this->Stop();
        // XXX need to use aMsg when it is provided
        this->OnError( kOpAsyncRead, aStatus );
    }

    // Close the output stream.
    if ( mOutputStream ) {
        rv = mOutputStream->Close();
        if ( NS_FAILED( rv ) ) {
            this->OnError( kOpOutputClose, rv );
        }
    }

    // Unhook input/output channels (don't need to cancel 'em).
    mInputChannel = 0;
    mOutputChannel = 0;

    // Notify observer that the download is complete.
    if ( !mError && mObserver ) {
        nsString msg(aMsg);
        rv = mObserver->Observe( (nsIStreamTransferOperation*)this,
                                  NS_ConvertASCIItoUCS2( NS_ISTREAMTRANSFER_CONTRACTID ";onCompletion" ).GetUnicode(),
                                  msg.GetUnicode() );
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
