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

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

nsFtpConnectionThread::nsFtpConnectionThread(PLEventQueue* aEventQ) {
	mEventQueue = aEventQ;
}

nsFtpConnectionThread::~nsFtpConnectionThread() {

}


// main event loop
NS_IMETHODIMP
nsFtpConnectionThread::Run() {
    nsresult rv;
    nsISocketTransport* lCPipe = nsnull;

    mState = FTP_CONNECT;

    NS_WITH_SERVICE(nsISocketTransportService, sts, kSocketTransportServiceCID, &rv);
    if(NS_FAILED(rv)) return rv;

    // Create the command channel transport
    const char *host;
    const PRInt32 port = 0;
    rv = mUrl->GetHost(&host);
    if (NS_FAILED(rv)) return rv;
    rv = mUrl->GetPort(port);
	if (NS_FAILED(rv)) return rv;

    rv = sts->CreateTransport(host, port, &lCPipe); // the command channel
    if (NS_FAILED(rv)) return rv;

    // get the output stream so we can write to the server
    rv = lCPipe->OpenOutputStream(&mOutStream);
    if (NS_FAILED(rv)) return rv;

    // get the input stream so we can read data from the server.
    rv = lCPipe->OpenInputStream(&mInStream);
    if (NS_FAILED(rv)) return rv;

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
            case FTP_READ_BUF:
                if (mState == mNextState)
                    NS_ASSERTION(0);

                rv = Read();
                mState = mNextState;
                break;
                // END: FTP_READ_BUF

            case FTP_S_USER:
                buffer = "USER anonymous";
                bufLen = PL_strlen(buffer);

                // send off the command
                rv = lOut->Write(buffer, bufLen, &bytes);
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
            
                rv = lOut->Write(buffer, bufLen, &bytes);
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
                    NS_ASSERTION(0);
                }
			    break;
			    // END: FTP_R_PASS    

		    case FTP_S_SYST:
			    buffer = "SYST\r\n";
                bufLen = PL_strlen(buffer);

			    // send off the command
                rv = mOut->(buffer, bufLen, &bytes);
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
				    if (mUseDefaultPath)
					    mState = FTP_S_PWD;
				    else
					    ; // ftp figure out what to do.

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
                bufLen = PL_strlen(buffer):

			    // send off the command
                rv = mOut->(buffer, bufLen, &bytes);
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
                rv = mOut->Write(buffer, bufLen, &bytes);

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
                // XXX mState figure out what to do
			    break;
			    // END: FTP_R_MACB

		    case FTP_S_PWD:
			    buffer = "PWD\r\n";
                bufLen = PL_strlen(buffer);

    		    // send off the command
                rv = mOut->Write(buffer, bufLen, &bytes);

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
			    PRInt32 start = mResponseMsg.Find('"', FALSE, 5);
			    nsString2 lNewMsg;
			    if (start > -1) {
				    mResponseMsg.Left(lNewMsg, start);
			    } else {
				    lNewMsg = mResponseMsg;				
			    }

			    // default next state
			    // mState = figure out what to do

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
					    mList = TRUE;
				    } else if (lNewMsg.Last() == ']') {
					    // path names ending with ']' imply vms
					    mServerType = FTP_VMS_TYPE;
					    mList = TRUE;
				    }
			    }

			    if (mUseDefaultPath && mServerType != FTP_VMS_TYPE) {
				    // we want to use the default path specified by the PWD command.
				    PRInt32 start = lNewMsg.Find('"', FALSE, 1);
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

            case FTP_COMPLETE:
            default:
                ;
        }    		
	}

    // Close the command channel
    lCPipe->CloseConnection();
}

nsresult
nsFtpConnectionThread::Read(void) {
    PRUint32 read, len;
    nsresult rv;
    char *buffer = nsnull;
    rv = mIn->GetLength(&len);
    if (NS_FAILURE(rv)) return rv;
    buffer = new char[len+1];
    if (!buffer) return 0; // XXX need a better return code
    rv = mIn->(buffer, len, &read);
    if (NS_FAILED(rv)) {
        delete [] buffer;
        return rv;
    }
    *aOutLen = read;

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
		mList = TRUE;
	}
	else if (mResponseMsg.Find("UNIX") > -1) {
		mServerType = FTP_UNIX_TYPE;
		mList = TRUE;
	}
	else if (mResponseMsg.Find("Windows_NT") > -1) {
		mServerType = FTP_NT_TYPE;
		mList = TRUE;
	}
	else if (mResponseMsg.Equals("VMS", 3)) {
		mServerType = FTP_VMS_TYPE;
		mList = TRUE;
	}
	else if (mResponseMsg.Equals("VMS/CMS", 6) || mResponseMsg.Equals("VM ", 3)) {
		mServerType = FTP_CMS_TYPE;
	}
	else if (mResponseMsg.Equals("DCTS", 4)) {
		mServerType = FTP_DCTS_TYPE;
	}
	else if (mResponseMsg.Find("MAC-OS TCP/Connect II") > -1) {
		mServerType = FTP_TCPC_TYPE;
		mList = TRUE;
	}
	else if (mResponseMsg.Equals("MACOS Peter's Server", 20)) {
		mServerType = FTP_PETER_LEWIS_TYPE;
		mList = TRUE;
	}
	else if (mResponseMsg.Equals("MACOS WebSTAR FTP", 17)) {
		mServerType = FTP_WEBSTAR_TYPE;
		mList = TRUE;
	}
}
