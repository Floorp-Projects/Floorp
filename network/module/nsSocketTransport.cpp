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
#include "nsRepository.h"
#include "nsNetStream.h"
#include "nsSocketTransport.h"
#include "nsINetService.h"
#include "nsINetlibURL.h"

#include "nsIServiceManager.h"
#include "nsXPComCIID.h"

#include "netutils.h"
#include "mktcp.h"
#include "sockstub.h"

#include "plstr.h"

extern nsIStreamListener* ns_NewStreamListenerProxy(nsIStreamListener* aListener, PLEventQueue* aEventQ);
static NS_DEFINE_IID(kIOutputStreamIID, NS_IOUTPUTSTREAM_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);

////////////////////////////////////////////////////////////////////////////
// from nsISupports

static NS_DEFINE_IID(kITransportIID, NS_ITRANSPORT_IID);
NS_IMPL_ADDREF(nsSocketTransport)
NS_IMPL_RELEASE(nsSocketTransport)
NS_IMPL_QUERY_INTERFACE(nsSocketTransport, kITransportIID);

////////////////////////////////////////////////////////////////////////////
// from nsITransport:

NS_METHOD 
nsSocketTransport::GetURL(nsIURL* *result) 
{
  *result = m_url;
  return NS_OK;
}


static NS_DEFINE_IID(kINetlibURLIID, NS_INETLIBURL_IID);

NS_METHOD nsSocketTransport::IsTransportOpen(PRBool * aSocketOpen)
{
	nsresult rv = NS_OK;
	if (aSocketOpen)
		*aSocketOpen = m_socketIsOpen;
	
	return rv;
}

/* Open means take the url and make a socket connection with the host...
   The user will then write data to be sent to the host via our input stream...
 */

NS_METHOD nsSocketTransport::Open(nsIURL *pURL)
{
  nsresult rv = NS_OK;

  // only actually try to do anything if we aren't open for business yet...
  if (m_socketIsOpen == PR_FALSE)
  {
	  m_socketIsOpen = PR_TRUE;

	  if (nsnull == pURL) 
		return NS_ERROR_NULL_POINTER;

	  if (nsnull == m_inputStreamConsumer)
		return NS_ERROR_NULL_POINTER;

	  NS_IF_ADDREF(pURL);
	  m_url = pURL;
      pURL->SetHostPort(m_port);
	  const char * hostName = nsnull;
	  pURL->GetHost(&hostName);
	  if (hostName)
	  {
		  PR_FREEIF(m_hostName);
		  m_hostName = PL_strdup(hostName);
	  }

	  // running this url will cause a connection to be made on the socket.
	  rv = NS_OpenURL(pURL, m_inputStreamConsumer);
	  m_socketIsOpen = PR_TRUE;
  }

  return rv;
}

NS_METHOD 
nsSocketTransport::SetInputStreamConsumer(nsIStreamListener* aListener)
{
  // generates the thread safe proxy
  m_inputStreamConsumer = ns_NewStreamListenerProxy(aListener, m_evQueue);  
  NS_IF_ADDREF(m_inputStreamConsumer);
  return NS_OK;
}


NS_METHOD
nsSocketTransport::GetOutputStreamConsumer(nsIStreamListener ** aConsumer)
{
	if (aConsumer)
	{
		// assuming the transport layer is a nsIStreamListener 
		*aConsumer = ns_NewStreamListenerProxy(this, m_evQueue); 
		// add ref bfore returning...
		NS_IF_ADDREF(*aConsumer);
	}
  return NS_OK;
}


NS_METHOD 
nsSocketTransport::GetOutputStream(nsIOutputStream ** aOutputStream)
{
  if (aOutputStream) {
    // a buffered stream supports the nsIOutputStream interface
	nsBufferedStream * stream = new nsBufferedStream();
	if (stream) // return the buffer in the format they want
		stream->QueryInterface(kIOutputStreamIID, (void **) aOutputStream);
	else
		*aOutputStream = NULL;
  }
  return NS_OK;
}


NS_IMETHODIMP 
nsSocketTransport::OnStartBinding(nsIURL* pURL, const char *aContentType)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsSocketTransport::OnProgress(nsIURL* pURL, 
			      PRUint32 aProgress, 
			      PRUint32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsSocketTransport::OnStatus(nsIURL* pURL, const PRUnichar* aMsg)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsSocketTransport::OnStopBinding(nsIURL* pURL, 
				 nsresult aStatus, 
				 const PRUnichar* aMsg)
{
// if the protocol instance called OnStopBinding then they are effectively asking us
// to close the connection. But they don't want US to go away.....
// so we need to close the underlying socket....
  CloseCurrentConnection();
  return NS_OK;
}

NS_IMETHODIMP 
nsSocketTransport::GetBindInfo(nsIURL* pURL,
			       nsStreamBindingInfo* aInfo)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsSocketTransport::OnDataAvailable(nsIURL* pURL, 
				   nsIInputStream *aIStream, 
				   PRUint32 aLength)
{
  nsresult rv = NS_OK;
  PRUint32 len, lenRead;
  URL_Struct* URL_s;
  m_ready_fd = NULL;

  /* XXX: HACK The following is a hack. 
   * Get the URL_s structure and pass it to sockstub protocol code to get the 
   * socket FD so that we could write the data back.
   */

  NS_PRECONDITION(m_socketIsOpen, "Uhoh, you tried to write to a socket before opening it by calling open...");

  if (m_socketIsOpen == PR_TRUE)  // only do something useful if we are open...
  {
  	  rv = GetURLInfo(pURL, &URL_s);

	  if (NS_SUCCEEDED(rv)) {
		if (nsnull != URL_s) {
		  /* Find the socket given URL_s pointer */
		  m_ready_fd = NET_GetSocketToHashTable(URL_s);
		}
	  } else {
		return rv;
	  }
    
	  if (m_ready_fd == NULL) {
		  return NS_ERROR_NULL_POINTER;
	  }

	  aIStream->GetLength(&len);

	  memset(m_buffer, '\0', NET_SOCKSTUB_BUF_SIZE);
	  while (len > 0) {
		if (len < NET_SOCKSTUB_BUF_SIZE) {
		  lenRead = len;
		}
		else {
		  lenRead = NET_SOCKSTUB_BUF_SIZE;
		}

		rv = aIStream->Read(m_buffer, 0, lenRead, &lenRead);
		if (NS_OK != rv) {
		  return rv;
		}

		/* XXX: We should check if the write has succeeded or not */
		(int) NET_BlockingWrite(m_ready_fd, m_buffer, lenRead);
		len -= lenRead;
	  }
  }
  else
	  rv = NS_ERROR_FAILURE; // stream was not open...

  return rv;
}



////////////////////////////////////////////////////////////////////////////
// from nsSocketTransport:

nsSocketTransport::nsSocketTransport(PRUint32 aPortToUse, const char * aHostName)
{
  NS_INIT_REFCNT();

  // remember the port and host name
  NS_PRECONDITION(aPortToUse > 0 && aHostName, "Creating a socket transport with an invalid port or host name.");
  m_port = aPortToUse;
  if (aHostName)
	m_hostName = PL_strdup(aHostName);
  else
	  m_hostName = PL_strdup("");

  m_inputStream = NULL;
  m_inputStreamConsumer = NULL;
  m_url = NULL;

  m_outStream = NULL;
  m_outStreamSize = 0;
  m_socketIsOpen = PR_FALSE; // initially, we have not opened the socket...

  /*
   * Cache the EventQueueService...
   */
  // XXX: What if this fails?
  
  mEventQService = nsnull;
  m_evQueue = nsnull;
  nsresult rv = nsServiceManager::GetService(kEventQueueServiceCID,
					     kIEventQueueServiceIID,
					     (nsISupports **)&mEventQService);

  if (nsnull != mEventQService) 
    mEventQService->GetThreadEventQueue(PR_GetCurrentThread(), &m_evQueue);
}

nsSocketTransport::~nsSocketTransport()
{
  nsresult rv = NS_OK;

  /* XXX: HACK The following is a hack. 
   * Get the URL_s structure and pass it to sockstub protocol code to delete it
   */

  rv = CloseCurrentConnection();

  NS_IF_RELEASE(mEventQService);
  NS_IF_RELEASE(m_outStream);
  NS_IF_RELEASE(m_url);
  NS_IF_RELEASE(m_inputStreamConsumer);
}

NS_IMETHODIMP 
nsSocketTransport::GetURLInfo(nsIURL* pURL, URL_Struct_ **aResult)
{
  nsresult rv = NS_OK;
  nsINetlibURL *pNetlibURL = NULL;

  NS_PRECONDITION(aResult != nsnull, "invalid input argument");
  if (aResult && pURL)
  {
	  *aResult = nsnull;
      if (pURL)
          rv = pURL->QueryInterface(kINetlibURLIID, (void**)&pNetlibURL);
	  if (NS_SUCCEEDED(rv) && pNetlibURL) {

		pNetlibURL->GetURLInfo(aResult);
		NS_RELEASE(pNetlibURL);
	  } 
  }
  
  return rv;
}

nsresult nsSocketTransport::CloseCurrentConnection()
{
	nsresult rv = NS_OK;
	URL_Struct_ * URL_s = nsnull;

	if (m_url)  // we want to interrupt this url...that will kill the underlying sockstub instance...
	{
		nsINetService * pNetService = nsnull;
		rv = NS_NewINetService(&pNetService, NULL);
		if (pNetService)
		{
			rv = pNetService->InterruptStream(m_url);
			NS_RELEASE(pNetService);
		}
		
	}

	// now free any per socket state information...
	m_socketIsOpen = PR_FALSE;

	NS_IF_RELEASE(m_url);
    m_url = nsnull;
    PR_FREEIF(m_hostName);
    return rv;
}