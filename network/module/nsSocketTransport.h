/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsSocketTransport_h__
#define nsSocketTransport_h__

#include "nsRepository.h"
#include "nsITransport.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIEventQueueService.h"
#include "nsAgg.h"
#include "nsString.h"

#define NET_SOCKSTUB_BUF_SIZE 1024

/* forward declaration */
struct URL_Struct_;

class nsSocketTransport : public nsITransport
{
public:
    ////////////////////////////////////////////////////////////////////////////
    // from nsISupports:

    NS_DECL_ISUPPORTS


    ////////////////////////////////////////////////////////////////////////////
    // from nsITransport:

    NS_IMETHOD GetURL(nsIURL* *result);

    NS_IMETHOD SetInputStreamConsumer(nsIStreamListener* aListener);

    NS_IMETHOD GetOutputStreamConsumer(nsIStreamListener ** aConsumer);

    NS_IMETHOD GetOutputStream(nsIOutputStream ** aOutputStream);

	NS_IMETHOD IsTransportOpen(PRBool * aSocketOpen);
	
	NS_IMETHOD Open(nsIURL * aUrl);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIStreamListener:

    NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
    NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax);
    NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg);
    NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg);
    NS_IMETHOD GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo);
    NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, 
                               PRUint32 aLength);


    ////////////////////////////////////////////////////////////////////////////
    // nsSocketTransport:

	// we have two types of connections: socket and file based....
    nsSocketTransport(PRUint32 aPortToUse, const char * aHostName);
	nsSocketTransport(const char * fileName); 

    virtual ~nsSocketTransport(void);
    NS_IMETHOD GetURLInfo(nsIURL* pURL, URL_Struct_ **aResult);

	// the following routines are called by the sock stub protocol hack....
	// we should be able to remove this dependency once we move things to the new
	// net lib world...
	PRUint32 GetPort() {return m_port;}
	const char * GetHostName() { return m_hostName;}
	const char * GetFileName() { return m_fileName;}

protected:

	// use this function to close the underlying connection....
	nsresult CloseCurrentConnection();
	// common initialization code..
	void Initialize();

	// socket specific information...
	PRUint32	m_port;
	char	   *m_hostName;
	char	   *m_fileName;
	PRBool	    m_socketIsOpen; // set when we have opened a socket....
	PRBool		m_isFileConnection;

    // the stream we write data from the socket into
    nsIInputStream * m_inputStream;  

	// XXX sock stub hack..need to remember stream with data to be written to the socket
	nsIInputStream * m_outStream; 
	PRUint32 m_outStreamSize;

    // the proxied stream listener. Whenever the transport layer
    // writes socket date to input stream, it calls OnDataAvailable
    // through the inputStreamConsumer any socket specific data
    nsIStreamListener *m_inputStreamConsumer; 

    nsIURL *m_url;		// the url we are currently running...
    nsString* mData;
    PRFileDesc *m_ready_fd;
    nsIEventQueueService* mEventQService;
    PLEventQueue* m_evQueue;
    char m_buffer[NET_SOCKSTUB_BUF_SIZE];
};

#endif // nsSocketTransport_h__
