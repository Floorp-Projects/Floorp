/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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

#include "nsFtpConnectionThread.h"
#include "nsFtpStreamListenerEvent.h" // the various events we fire off to the
                                      // owning thread.
#include "nsITransport.h"
#include "nsISocketTransportService.h"
#include "nsIUrl.h"

#include "nsIInputStream.h"

#include "prcmon.h"
#include "prprf.h"
#include "netCore.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);


NS_IMPL_ADDREF(nsFtpConnectionThread);
NS_IMPL_RELEASE(nsFtpConnectionThread);

NS_IMETHODIMP
nsFtpConnectionThread::QueryInterface(const nsIID& aIID, void** aInstancePtr) {
    NS_ASSERTION(aInstancePtr, "no instance pointer");
    if (aIID.Equals(nsIRunnable::GetIID()) ||
        aIID.Equals(kISupportsIID) ) {
        *aInstancePtr = NS_STATIC_CAST(nsFtpConnectionThread*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    /*if (aIID.Equals(nsIStreamListener::GetIID()) ||
        aIID.Equals(nsIStreamObserver::GetIID())) {
        *aInstancePtr = NS_STATIC_CAST(nsIStreamListener*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }*/
    return NS_NOINTERFACE; 
}

nsFtpConnectionThread::nsFtpConnectionThread(PLEventQueue* aEventQ, nsIStreamListener* aListener) {
	mEventQueue = aEventQ; // whoever creates us must provide an event queue
                           // so we can post events back to them.
    mListener = aListener;
    NS_ADDREF(mListener);
    mAction = GET;
    mState = FTP_S_USER;
}

nsFtpConnectionThread::~nsFtpConnectionThread() {
    NS_IF_RELEASE(mListener);
}


// main event loop
NS_IMETHODIMP
nsFtpConnectionThread::Run() {
    nsresult rv;
    nsITransport* lCPipe = nsnull;
    nsITransport* lDPipe = nsnull;

    mState = FTP_S_USER;

    NS_WITH_SERVICE(nsISocketTransportService, sts, kSocketTransportServiceCID, &rv);
    if(NS_FAILED(rv)) return rv;

    // Create the command channel transport
    const char *host;
    PRInt32 port = 0;
    rv = mUrl->GetHost(&host);
    if (NS_FAILED(rv)) return rv;
    rv = mUrl->GetPort(&port);
	if (NS_FAILED(rv)) return rv;

    rv = sts->CreateTransport(host, port, &lCPipe); // the command channel
    if (NS_FAILED(rv)) return rv;

    // get the output stream so we can write to the server
    rv = lCPipe->OpenOutputStream(&mCOutStream);
    if (NS_FAILED(rv)) return rv;

    // get the input stream so we can read data from the server.
    rv = lCPipe->OpenInputStream(&mCInStream);
    if (NS_FAILED(rv)) return rv;


    // tell the user that we've begun the transaction.
    nsFtpOnStartBindingEvent* event =
        new nsFtpOnStartBindingEvent(mListener, nsnull);
    if (event == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = event->Fire(mEventQueue);
    if (NS_FAILED(rv)) {
        delete event;
        return rv;
    }

	while (1) {
        nsresult rv;
        char *buffer = nsnull;      // the buffer to be sent to the server
        PRUint32 bufLen = 0;
        PRUint32 bytes = 0;

        // each hunk of data that comes in is evaluated, appropriate action 
        // is taken and the state is incremented.

	    // XXX some of the "buffer"s allocated below in the individual states can be removed
	    // XXX and replaced with static char or #defines.
        switch(mState) {

            // Reading state. used after a write in order to retrieve
            // the response from the server.
            case FTP_READ_BUF:
                if (mState == mNextState)
                    NS_ASSERTION(0, "ftp read state mixup");

                rv = Read();
                mState = mNextState;
                break;
                // END: FTP_READ_BUF

//////////////////////////////
//// CONNECTION SETUP STATES
//////////////////////////////


            case FTP_S_USER:
                buffer = "USER anonymous";
                bufLen = PL_strlen(buffer);

                // send off the command
                rv = mCOutStream->Write(buffer, bufLen, &bytes);
                if (NS_FAILED(rv)) return rv;

                if (bytes < bufLen) {
                    // didn't write everything, try again
                    // XXX remember how much data was written and send the amount left
                    break;
                }
                mState = FTP_READ_BUF;
                mNextState = FTP_R_USER;
                break;
                // END: FTP_S_USER

            case FTP_R_USER:
                if (mResponseCode == 3) {
                    // send off the password
                    mState = FTP_S_PASS;
                } else if (mResponseCode == 2) {
                    // no password required, we're already logged in
				    mState = FTP_S_SYST;
                }
                break;
			    // END: FTP_R_USER

            case FTP_S_PASS:
                if (!mPassword.Length()) {
                    // XXX we need to prompt the user to enter a password.

                    // sendEventToUIThreadToPostADialog(&mPassword);
                }
                buffer = "PASS guest\r\n";
                bufLen = PL_strlen(buffer);
                // PR_smprintf(buffer, "PASS %.256s\r\n", mPassword);
            
                rv = mCOutStream->Write(buffer, bufLen, &bytes);
                if (NS_FAILED(rv)) return rv;

                if (bytes < bufLen) {
                    break;
                }
                mState = FTP_READ_BUF;
                mNextState = FTP_R_PASS;
                break;
                // END: FTP_S_PASS

            case FTP_R_PASS:
                if (mResponseCode == 3) {
                    // send account info
                    mState = FTP_S_ACCT;
                } else if (mResponseCode == 2) {
                    // logged in
                    mState = FTP_S_SYST;
                } else {
                    NS_ASSERTION(0, "ftp unexpected condition");
                }
			    break;
			    // END: FTP_R_PASS    

		    case FTP_S_SYST:
			    buffer = "SYST\r\n";
                bufLen = PL_strlen(buffer);

			    // send off the command
                rv = mCOutStream->Write(buffer, bufLen, &bytes);
                if (NS_FAILED(rv)) return rv;

                if (bytes < bufLen) {
                    break;
                }

                mState = FTP_READ_BUF;
			    mNextState = FTP_R_SYST;
			    break;
                // END: FTP_S_SYST

		    case FTP_R_SYST:
			    if (mResponseCode == 2) {
                    if (mUseDefaultPath) {
					    mState = FTP_S_PWD;
                    } else {
                        mState = FindActionState();
                    }

				    SetSystInternals(); // must be called first to setup member vars.

				    // setup next state based on server type.
				    if (mServerType == FTP_PETER_LEWIS_TYPE || mServerType == FTP_WEBSTAR_TYPE) {
					    mState = FTP_S_MACB;
				    } else if (mServerType == FTP_TCPC_TYPE || mServerType == FTP_GENERIC_TYPE) {
					    mState = FTP_S_PWD;
				    } 
			    } else {
				    mState = FTP_S_PWD;		
			    }
			    break;
			    // END: FTP_R_SYST

		    case FTP_S_ACCT:
			    buffer = "ACCT noaccount\r\n";
                bufLen = PL_strlen(buffer);

			    // send off the command
                rv = mCOutStream->Write(buffer, bufLen, &bytes);
                if (NS_FAILED(rv)) return rv;

                if (bytes < bufLen) {
                    break;
                }

                mState = FTP_READ_BUF;
			    mNextState = FTP_R_ACCT;			
			    break;
                // END: FTP_S_ACCT

		    case FTP_R_ACCT:
			    if (mResponseCode == 2) {
				    mState = FTP_S_SYST;
			    } else {
				    // failure. couldn't login
				    // XXX use a more descriptive error code.
				    return NS_ERROR_NOT_IMPLEMENTED;
			    }
			    break;
			    // END: FTP_R_ACCT

		    case FTP_S_MACB:
			    buffer = "MACB ENABLE\r\n";
                bufLen = PL_strlen(buffer);

                // send off the command
                rv = mCOutStream->Write(buffer, bufLen, &bytes);

                if (bytes < bufLen) {
                    break;
                }

                mState = FTP_READ_BUF;
			    mNextState = FTP_R_MACB;
			    break;
                // END: FTP_S_MACB

		    case FTP_R_MACB:
			    if (mResponseCode == 2) {
				    // set the mac binary
				    if (mServerType == FTP_UNIX_TYPE) {
					    // This state is carry over from the old ftp implementation
					    // I'm not sure what's really going on here.
					    // original comment "we were unsure here"
					    mServerType = FTP_NCSA_TYPE;	
				    }
			    }
                mState = FindActionState();
			    break;
			    // END: FTP_R_MACB

		    case FTP_S_PWD:
			    buffer = "PWD\r\n";
                bufLen = PL_strlen(buffer);

    		    // send off the command
                rv = mCOutStream->Write(buffer, bufLen, &bytes);

                if (bytes < bufLen) {
                    break;
                }
                
                mState = FTP_READ_BUF;
			    mNextState = FTP_R_PWD;
			    break;
                // END: FTP_S_PWD

		    case FTP_R_PWD:
			    {
			    // fun response interpretation begins :)
			    PRInt32 start = mResponseMsg.Find('"', PR_FALSE, 5);
			    nsString2 lNewMsg;
			    if (start > -1) {
				    mResponseMsg.Left(lNewMsg, start);
			    } else {
				    lNewMsg = mResponseMsg;				
			    }

			    // default next state
			    mState = FindActionState();

			    // reset server types if necessary
			    if (mServerType == FTP_TCPC_TYPE) {
				    if (lNewMsg.CharAt(1) == '/') {
					    mServerType = FTP_NCSA_TYPE;
				    }
			    }
			    else if(mServerType == FTP_GENERIC_TYPE) {
				    if (lNewMsg.CharAt(1) == '/') {
					    // path names ending with '/' imply unix
					    mServerType = FTP_UNIX_TYPE;
					    mList = PR_TRUE;
				    } else if (lNewMsg.Last() == ']') {
					    // path names ending with ']' imply vms
					    mServerType = FTP_VMS_TYPE;
					    mList = PR_TRUE;
				    }
			    }

			    if (mUseDefaultPath && mServerType != FTP_VMS_TYPE) {
				    // we want to use the default path specified by the PWD command.
				    PRInt32 start = lNewMsg.Find('"', PR_FALSE, 1);
				    nsString2 path, ptr;
				    lNewMsg.Right(path, start);

				    if (path.First() != '/') {
					    start = path.Find('/');
					    if (start > -1) {
						    path.Right(ptr, start);
					    } else {
						    // if we couldn't find a slash, check for back slashes and switch them out.
						    PRInt32 start = path.Find('\\');
						    if (start > -1) {
							    path.ReplaceChar('\\', '/');
						    }
					    }
				    } else {
					    ptr = path;
				    }

				    // construct the new url
				    if (ptr.Length()) {
					    nsString2 newPath;

					    newPath = ptr;


					    const char *initialPath = nsnull;
					    rv = mUrl->GetPath(&initialPath);
					    if (NS_FAILED(rv)) return rv;
					    
					    if (initialPath && *initialPath) {
						    if (newPath.Last() == '/')
							    newPath.Cut(newPath.Length()-1, 1);
						    newPath.Append(initialPath);
					    }

					    char *p = newPath.ToNewCString();
					    mUrl->SetPath(p);
					    delete [] p;
				    }
			    }

			    // change state for these servers.
			    if (mServerType == FTP_GENERIC_TYPE
				    || mServerType == FTP_NCSA_TYPE
				    || mServerType == FTP_TCPC_TYPE
				    || mServerType == FTP_WEBSTAR_TYPE
				    || mServerType == FTP_PETER_LEWIS_TYPE)
				    mState = FTP_S_MACB;

			    break;
			    }
			    // END: FTP_R_PWD


//////////////////////////////
//// DATA CONNECTION STATES
//////////////////////////////
            case FTP_S_PASV:
                buffer = "PASV\r\n";
                bufLen = PL_strlen(buffer);

                rv = mCOutStream->Write(buffer, bufLen, &bytes);

                if (bytes < bufLen) {
                    break;
                }
                mState = FTP_READ_BUF;
                mNextState = FTP_R_PASV;
                break;
                // FTP: FTP_S_PASV

            case FTP_R_PASV:
                {
                PRInt32 h0, h1, h2, h3, p0, p1;
                PRInt32 port;

                if (mResponseCode != 2)  {
                    // failed. increment to port.
                    // mState = FTP_S_PORT;

                    mUsePasv = PR_FALSE;
                    // until we have PORT support, we'll just fail.
                    mState = FTP_ERROR;
                    break;
                }

                mUsePasv = PR_TRUE;

                char *ptr = nsnull;
                char *response = mResponseMsg.ToNewCString();
                if (!response)
                    return NS_ERROR_OUT_OF_MEMORY;

                // The returned address string can be of the form
                // (xxx,xxx,xxx,xxx,ppp,ppp) or
                //  xxx,xxx,xxx,xxx,ppp,ppp (without parens)
                ptr = response;
                while (*ptr++) {
                    if (*ptr == '(') {
                        // move just beyond the open paren
                        ptr++;
                        break;
                    }
                    if (*ptr == ',') {
                        ptr--;
                        // backup to the start of the digits
                        while ( (*ptr >= '0') && (*ptr <= '9'))
                            ptr--;
                        ptr++; // get back onto the numbers
                        break;
                    }
                } // end while

                PRInt32 fields = PR_sscanf(ptr, 
#ifdef __alpha
		            "%d,%d,%d,%d,%d,%d",
#else
		            "%ld,%ld,%ld,%ld,%ld,%ld",
#endif
		            &h0, &h1, &h2, &h3, &p0, &p1);

                if (fields < 6) {
                    // bad format. we'll try PORT, but it's probably over.
                    
                    mUsePasv = PR_FALSE;
                    // until we have port support, we'll just fail
                    //mState = FTP_S_PORT;
                    mState = FTP_ERROR;
                    break;
                }

                port = ((PRInt32) (p0<<8)) + p1;
                char host[17];
                PR_smprintf(host, "%ld.%ld.%ld.%ld", h0, h1, h2, h3);

                // now we know where to connect our data channel
                rv = sts->CreateTransport(host, port, &lDPipe); // the command channel
                if (NS_FAILED(rv)) return rv;

                if (mAction == GET) {
                    // get the input stream so we can read data from the server.
                    rv = lDPipe->OpenInputStream(&mDInStream);
                    if (NS_FAILED(rv)) return rv;
                } else {
                    // get the output stream so we can write to the server
                    rv = lDPipe->OpenOutputStream(&mDOutStream);
                    if (NS_FAILED(rv)) return rv;
                }

                // we're connected figure out what type of transfer we're doing (ascii or binary)

                break;
                }
                // FTP: FTP_R_PASV

//////////////////////////////
//// ACTION STATES
//////////////////////////////
            case FTP_S_DEL_FILE:
                {
                const char *filename = nsnull;
                nsresult rv;
                rv = mUrl->GetPath(&filename); // XXX we should probably check to 
                                               // XXX make sure we have an actual filename.
                if (NS_FAILED(rv)) return rv;
                PR_smprintf(buffer, "DELE %s\r\n", filename);
                bufLen = PL_strlen(buffer);

    		    // send off the command
                rv = mCOutStream->Write(buffer, bufLen, &bytes);

                if (bytes < bufLen) {
                    break;
                }
                
                mState = FTP_READ_BUF;
			    mNextState = FTP_R_DEL_FILE;
                break;
                }
                // END: FTP_S_DEL_FILE

            case FTP_R_DEL_FILE:
                if (mResponseCode != 2) {
                    // failed. Increment to the dir delete.
                    mState = FTP_S_DEL_DIR;
                    break;
                }

                mState = FTP_COMPLETE;
                break;
                // END: FTP_R_DEL_FILE

            case FTP_S_DEL_DIR:
                {
                const char *dir = nsnull;
                nsresult rv;
                rv = mUrl->GetPath(&dir);
                if (NS_FAILED(rv)) return rv;
                PR_smprintf(buffer, "RMD %s\r\n", dir);
                bufLen = PL_strlen(buffer);

    		    // send off the command
                rv = mCOutStream->Write(buffer, bufLen, &bytes);

                if (bytes < bufLen) {
                    break;
                }
                
                mState = FTP_READ_BUF;
			    mNextState = FTP_R_DEL_DIR;

                break;
                }
                // END: FTP_S_DEL_DIR

            case FTP_R_DEL_DIR:
                if (mResponseCode != 2) {
                    // failed.
                    // XXX indicate failure
                }
                mState = FTP_COMPLETE;
                break;
                // END: FTP_R_DEL_DIR

            case FTP_S_MKDIR:
                {
                const char *dir = nsnull;
                nsresult rv;
                rv = mUrl->GetPath(&dir);
                if (NS_FAILED(rv)) return rv;
                PR_smprintf(buffer, "MKD %s\r\n", dir);
                bufLen = PL_strlen(buffer);

    		    // send off the command
                rv = mCOutStream->Write(buffer, bufLen, &bytes);

                if (bytes < bufLen) {
                    break;
                }
                
                mState = FTP_READ_BUF;
			    mNextState = FTP_R_MKDIR;
                break;
                }
                // END: FTP_S_MKDIR

            case FTP_R_MKDIR:
                if (mResponseCode != 2) {
                    // XXX indicate failure
                }
                mState = FTP_COMPLETE;
                break;
                // END: FTP_R_MKDIR

            default:
                ;

        } // END: switch 
    } // END: event loop/message pump/while loop

    // Close the data channel
    NS_IF_RELEASE(lDPipe);

    // Close the command channel
    NS_RELEASE(lCPipe);
}

nsresult
nsFtpConnectionThread::Init(nsIThread* aThread) {
/*    mThread = aThread;
    NS_ADDREF(mThread);

    PRThread* prthread;
    aThread->GetPRThread(&prthread);
    PR_CEnterMonitor(this);
    mEventQueue = PL_CreateEventQueue("ftp worker thread event loop", prthread);
    // wake up event loop
    PR_CNotify(this);
    PR_CExitMonitor(this);
*/
    return NS_OK;
}

nsresult
nsFtpConnectionThread::SetAction(FTP_ACTION aAction) {
    if (mConnected)
        return NS_ERROR_ALREADY_CONNECTED;
    mAction = aAction;
    return NS_OK;
}

nsresult
nsFtpConnectionThread::SetUsePasv(PRBool aUsePasv) {
    if (mConnected)
        return NS_ERROR_ALREADY_CONNECTED;
    mUsePasv = aUsePasv;
    return NS_OK;
}


nsresult
nsFtpConnectionThread::Read(void) {
    PRUint32 read, len;
    nsresult rv;
    char *buffer = nsnull;
    rv = mCInStream->GetLength(&len);
    if (NS_FAILED(rv)) return rv;
    buffer = new char[len+1];
    if (!buffer) return 0; // XXX need a better return code
    rv = mCInStream->Read(buffer, len, &read);
    if (NS_FAILED(rv)) {
        delete [] buffer;
        return rv;
    }

	// get the response code out.
    PR_sscanf(buffer, "%d", &mResponseCode);

	// get the rest of the line
	mResponseMsg = buffer+4;
    return rv;   
}

// Here's where we do all the string whacking/parsing magic to determine
// what type of server it is we're dealing with.
void
nsFtpConnectionThread::SetSystInternals(void) {
    if (mResponseMsg.Equals("UNIX Type: L8 MAC-OS MachTen", 28)) {
		mServerType = FTP_MACHTEN_TYPE;
		mList = PR_TRUE;
	}
	else if (mResponseMsg.Find("UNIX") > -1) {
		mServerType = FTP_UNIX_TYPE;
		mList = PR_TRUE;
	}
	else if (mResponseMsg.Find("Windows_NT") > -1) {
		mServerType = FTP_NT_TYPE;
		mList = PR_TRUE;
	}
	else if (mResponseMsg.Equals("VMS", 3)) {
		mServerType = FTP_VMS_TYPE;
		mList = PR_TRUE;
	}
	else if (mResponseMsg.Equals("VMS/CMS", 6) || mResponseMsg.Equals("VM ", 3)) {
		mServerType = FTP_CMS_TYPE;
	}
	else if (mResponseMsg.Equals("DCTS", 4)) {
		mServerType = FTP_DCTS_TYPE;
	}
	else if (mResponseMsg.Find("MAC-OS TCP/Connect II") > -1) {
		mServerType = FTP_TCPC_TYPE;
		mList = PR_TRUE;
	}
	else if (mResponseMsg.Equals("MACOS Peter's Server", 20)) {
		mServerType = FTP_PETER_LEWIS_TYPE;
		mList = PR_TRUE;
	}
	else if (mResponseMsg.Equals("MACOS WebSTAR FTP", 17)) {
		mServerType = FTP_WEBSTAR_TYPE;
		mList = PR_TRUE;
	}
}

FTP_STATE
nsFtpConnectionThread::FindActionState(void) {

    // These operations require the separate data channel.
    if (mAction == GET || mAction == POST) {
        // we're doing an operation that requies the data channel.
        // figure out what kind of data channel we want to setup,
        // and do it.
        if (mUsePasv)
            return FTP_S_PASV;
        else
            // until we have PORT support, we'll just fail.
            // return FTP_S_PORT;
            return FTP_ERROR;
    }

    // These operations use the command channel response as the
    // data to return to the user.
    if (mAction == DEL)
        return FTP_S_DEL_FILE; // we assume it's a file

    if (mAction == MKDIR)
        return FTP_S_MKDIR;

    return FTP_ERROR;
}
