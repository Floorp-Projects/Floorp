/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsITransport_h___
#define nsITransport_h___

#include "nscore.h"
#include "nsISupports.h"
#include "prio.h"
#include "nsIStreamListener.h"
#include "nsIOutputStream.h"
#include "nsIURL.h"

#define NS_ITRANSPORT_IID           \
{ 0x54ae75a0, 0x99e8, 0x11d2, \
  {0xb9, 0x47, 0x00, 0x80, 0x5f, 0x52, 0x35, 0x1a} }

class nsITransport : public nsIStreamListener {
public:

  /** Accessors */

  /**
   * Returns the URL that is associated with the Transport.
   *
   * If the transport doesn't have URL associated with it 
   * because it was constructed by passing host and port, 
   * it will return NULL.
   *
   * @param result   the URL that was used to build the transport
   * @return The return value is currently ignored.
   */
  NS_IMETHOD GetURL(nsIURL* *result) = 0;

  /**
   * Sets up the InputStreamListener. Transport layer will notify this 
   * consumer when it writes data from the socket into its input stream.
   *
   * @param aListener:  The InputStreamConsumer provided by the protocol
   *       instance which the transport layer should use for input stream
   *       notifications. The transport layer assumes responsibility for
   *       building a stream listener proxy for the protocol's listener.
   *
   * @return The return value is currently ignored.
   */
  NS_IMETHOD SetInputStreamConsumer(nsIStreamListener* listener) = 0;

  /**
  * Returns a proxied stream listener which the transport layer expects
  * notifications through for the output stream. The protocol will bind the 
  * listener returned to its output stream where it writes data it wants 
  * sent through the transport layer.
  * As with the input stream consumer case, the transport layer returns a
  * proxied consumer.
  *
  * @return aConsumer: The proxied transport layer's consumer for the
  * protocol's output stream.
  */

  NS_IMETHOD GetOutputStreamConsumer(nsIStreamListener ** aConsumer) = 0;

  /** 
  * Returns an output stream which the protocol instance should write data 
  * it wants transmitted through the transport layer into. After writing data 
  * to this stream, the protocol should notify the output stream consumer. 
  * This informs the transport layer that data is available. 
  * 
  * @param aOutputStream: The output stream the protocol will write data for 
  *        transmission into. 
  * 
  * @return NS_FAILURE if the stream could not be returned. 
  */ 

  NS_IMETHOD GetOutputStream(nsIOutputStream ** aOutputStream) = 0; 

  /**
  * Returns true if the transport has been opened already and FALSE otherwise.
  * If a transport has not been opened yet, then you should open it using
  * Open(nsIURL *).
  **/
  
  NS_IMETHOD IsTransportOpen(PRBool * aSocketOpen) = 0;

  /** 
  * This replaces LoadUrl. Before using a transport, you should open the transport.
  * This functionality may go away after we remove the reliance on sock stub. You
  * should always call open the first time you want to use the transport in order to
  * open its underlying object (file, socket, etc.). You can later issue a ::StopBinding
  * call on the transport which should cause the underlying object (file, socket, etc.) to
  * be closed. You could then open the transport again with another URL. Note, for sockets,
  * if you wish to re-use a connection, you shouldn't call StopBinding on it as this would
  * cause the connection to be closed...
  **/ 
  
  NS_IMETHOD Open(nsIURL * aUrl) = 0;
};


#endif /* nsITransport_h___ */
