/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Security Services for Java.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

// This demonstrates the SSL client-side support
//

package org.mozilla.jss.ssl;

import java.io.*;
import java.util.*;
import java.net.*;
import org.mozilla.jss.*;
import org.mozilla.jss.crypto.AlreadyInitializedException;

/**
 * Parameters supported by this socket test:
 * 
 * filename 	file to be read from https server (default: /index.html)
 * port 	port to connect to (default: 443)
 * ipaddr 	address to connect to (overrides hostname, no default)
 * hostname 	host to connect to (no default)
 * clientauth 	do client-auth or not (default: no client-auth)
 *
 * The following parameters are used for regression testing, so
 * we can print success or failure of the test.
 * 
 * filesize 	size of file to be read
 * status 	security status of connection - this has to be an integer
 * cipher 	
 * sessionKeySize
 * sessionSecretSize
 * issuer
 * subject
 * certSerialNum
 * 
 */

public class SSLClient 
{
    boolean     handshakeEventHappened = false;
    boolean     doClientAuth = false;
    Hashtable   args;
    PrintStream results;
    String      versionStr;

  String argNames[] = {
    "filename",
    "port",
    "ipaddr",
    "hostname",
    "filesize",
    "status",
    "sessionKeySize",
    "sessionSecretSize",
    "cipher",
    "issuer",
    "subject",
    "certSerialNum",
  };
  
  String values[] = {
    "/index",    	 // filename
    "443",	         // port to connect to
    "",			 // ipaddr (use hostname instead)
    "www.amazon.com", // hostname to connect to
    "1024",		 // filesize
    "2",	       	 // status, 2 means ???
    "128",		 // expected session key size
    "128",		 // expected session key secret bits
    "RC48",		 // expected cipher
    "CN=Hardcore Certificate Server II, OU=Hardcore, O=Netscape Communications Corporation, C=US", // expected issuer DN of server
    "CN=hbombsgi.mcom.com, OU=Hardcore, C=US", // expected subject DN of server
    "00C3",		 // serial number
    };
  
  String okay = "okay";
  String failed = "FAILED";
  
  private static String htmlHeader = "SSL Client Tester";
  private static String htmlTail   = "\n";
  
  /* simple helper functions */

  private boolean isInvalid(String s) {
    return (s == null) || s.equals("");
  }
  
  private String getArgument(String key) {
    return (String) args.get(key);
  }
  
  /*
   * return "okay" or "FAILED" based on equality of
   * the argument strings
   */
  private String cmp(String s1, String s2) {
    if(s1 == s2) return okay;
    if(s1 == null) return failed;
    if(s1.equals(s2)) return okay;
    return failed;
  }
  
  private String cmp(String s1, int s2) {
    return cmp(s1, new Integer(s2).toString());
  }
  
  
  public void run(boolean printHeader) 
    {
      try {
	SSLHandshakeCompletedListener listener = null;
	
	if(printHeader)
	  results.println(htmlHeader);
	results.println("SSL Client Tester");
	results.println(
			"$Id: SSLClient.java,v 1.2 2001/02/09 11:26:09 nicolson%netscape.com Exp $ " + 
			versionStr );
	
	SSLSocket s;
	String    hostname;
	int       port;
	
	String    filename = getArgument("filename");

	if(isInvalid(filename)) {
	  filename = "/index.html";
	}

	String msg = "GET " + filename;
	
	String portstr = getArgument("port");

	if(isInvalid(portstr)) {
	  port = 443;
	} else {
	  port = Integer.valueOf(portstr).intValue();
	}
	
	String addrstr   = getArgument("ipaddr");
	hostname         = addrstr;    // unless it gets changed
	
	String tmpStr    = getArgument("clientauth");
	if(isInvalid(tmpStr))
	  doClientAuth = false;
	else {
	  tmpStr       = tmpStr.toLowerCase();
	  doClientAuth = !(tmpStr.equals("off") ||
			   tmpStr.equals("false") ||
			   tmpStr.equals("0"));
	}
	
	
	if(isInvalid(addrstr)) {
	  // check for a host name 
	  hostname = getArgument("hostname");
	  if(isInvalid(hostname)) {
	    throw new Exception("hostname not specified");
	  }
	}
	results.println("Connecting to " + hostname +
			" on port " + port );
	
	SSLCertificateApprovalCallback approvalCallback = new TestCertApprovalCallback();
	SSLClientCertificateSelectionCallback certSelectionCallback = new TestClientCertificateSelectionCallback();
	s = new SSLSocket(InetAddress.getByName(hostname),
						port,
						null,   /* local addr */
						0,      /* local port */
						true,   /* stream     */
						approvalCallback,
						certSelectionCallback
					);
						
	results.println("Connected.");
	
	// select the cert for client auth
        // You will have to provide a certificate with this
        // name if you expect client auth to work.
	
	//s.setClientCertNickname("JavaSSLTestClientCert");
	
	
        // Setup a hanshake callback. This listener will get invoked
        // When the SSL handshake is completed on this socket.
	
	listener = new ClientHandshakeCB(this);
	s.addHandshakeCompletedListener(listener);

    //s.forceHandshake();
	
	OutputStream o = s.getOutputStream();
	
	PrintOutputStreamWriter out = new PrintOutputStreamWriter(o);
	
	results.println("Sending: " + msg + " to " + hostname +
			", " + port );
	
	// send HTTP GET message across SSL connection
	out.println(msg + "\r");
	
	InputStream in         = s.getInputStream();
	byte[]      bytes      = new byte[4096];
	int         totalBytes = 0;
	int         numReads   = 0;
	String      lastBytes  = null;
	
	
	// now try to read data back from the SSL connection
	try {
	  for(;;) {
	    results.println("Calling Read.");
	    int n = in.read(bytes, 0, bytes.length);
	    if(n == -1) {
	      results.println("EOF found.");
	      break;
	    }
	    
	    if(n == 0) {
	      results.println("Zero bytes read?");
	      break;
	    }
	    numReads++;
	    
	    if(totalBytes == 0) {
	      // don't print forever...
	      String data = new String(bytes, 0, 30, "8859_1");
	      results.println("Read " + n + " bytes of data");
	      results.println("First 30 bytes: " + escapeHTML(data));
	    }
	    
	    totalBytes += n;
	    lastBytes = new String(bytes, n-31, 30, "8859_1");
	  }
	} catch (IOException e) {
	  results.println(
			  "IOException while reading from pipe?  Actually got " + 
			  totalBytes + " bytes total");
	  e.printStackTrace(results);
	  results.println("");
	  throw e;
	} finally {
	  results.println("Last 30 bytes: " + lastBytes);
	  results.println("Number of read() calls: " + numReads );
	  
	  
	  /*
	   * if you want to test sslimpl.c's nsn_ThrowError(), try
	   * uncommenting the following line.  This will cause the
	   * getStatus() call to fail.
	   */
	  
	  // in.close();
	  
	  results.println("Diagnostics");
	  String tmp;
	  SSLSecurityStatus status = s.getStatus();
	  
	  results.println("Total bytes read: " + totalBytes );
	  results.println("Security status of session:");
	  results.println(status.toString());

	  // now, for the regression testing stuff
	  
	if (false) {
	  results.println("Regression Tests");
	  
	  results.println("Handshake callback event happened: " +
			  ((handshakeEventHappened) ? okay : failed));
	  
	  if(!isInvalid(tmp = getArgument("filesize"))) {
	    results.println("filesize: " + cmp(tmp, totalBytes));
	  }
	  if(!isInvalid(tmp = getArgument("status"))) {
	    results.println("status: " +
			    cmp(tmp, status.getSecurityStatus()));
	  }
	  if(!isInvalid(tmp = getArgument("sessionKeySize"))) {
	    results.println("sessionKeySize: " +
			    cmp(tmp, status.getSessionKeySize()));
	  }
	  if(!isInvalid(tmp = getArgument("sessionSecretSize"))) {
	    results.println("sessionSecretSize: " +
			    cmp(tmp, status.getSessionSecretSize()));
	  }
	  if(!isInvalid(tmp = getArgument("cipher"))) {
	    results.println("cipher: " +
			    cmp(tmp, status.getCipher()));
	  }
	  if(!isInvalid(tmp = getArgument("issuer"))) {
	    results.println("issuer: " +
			    cmp(tmp, status.getRemoteIssuer()));
	  }
	  if(!isInvalid(tmp = getArgument("subject"))) {
	    results.println("subject: " +
			    cmp(tmp, status.getRemoteSubject()));
	  }
	  if(!isInvalid(tmp = getArgument("certSerialNum"))) {
	    String serialNum = status.getSerialNumber();
	    results.println("certSerialNum: " +
			    cmp(tmp, serialNum));
	  }
	} // if false
	}
	// Got here, so no exception thrown above.
	// Try to clean up.
	    o.close();
	    o = null;
	    in.close();
	    in = null;
	    if (listener != null) {
	      s.removeHandshakeCompletedListener(listener);
	      listener = null;
	    }
	    s.close();
	    s = null;
	    
	    
      } catch(Exception e) {
	results.println("***** TEST FAILED *****");
	e.printStackTrace(results);
	results.println("If there is no stack trace, try disabling the JIT and trying again.");
      }
      results.println("END OF TEST");
    }
  
  /**
   * given an input string, convert less-than, greater-than, and ampersand
   * from raw characters to escaped characters
   * (&lt; becomes `&amp;lt;', etc.)
   */
  private String escapeHTML(String s) 
    {
      StringBuffer result = new StringBuffer();
      
      // this is inefficient, but I don't care
      for(int i=0; i<s.length(); i++) {
	char c = s.charAt(i);
	switch(c) {
	case '<':	result.append("&lt;");	break;
	case '>':	result.append("&gt;");	break;
	case '&':	result.append("&amp;"); break;
	default:	result.append(c);	break;
	}
      }
      
      return result.toString();
    }
  
  public SSLClient( PrintStream ps, String verStr)
    {
      this.args       = new Hashtable();
      this.results    = ps;
      this.versionStr = verStr;
      
      for(int i=0; i<argNames.length; i++) {
	String value = values[i];
	if(value != null)
	  this.args.put(argNames[i], value);
      }
    }
  
  static final int cipherSuites[] = {
    SSLSocket.SSL3_RSA_WITH_RC4_128_MD5,
    SSLSocket.SSL3_RSA_WITH_3DES_EDE_CBC_SHA,
    SSLSocket.SSL3_RSA_WITH_DES_CBC_SHA,
    SSLSocket.SSL3_RSA_EXPORT_WITH_RC4_40_MD5,
    SSLSocket.SSL3_RSA_EXPORT_WITH_RC2_CBC_40_MD5,
    SSLSocket.SSL3_RSA_WITH_NULL_MD5,
    0
  };
  
  public static void main(String argv[]) 
    {
      int i;

      try {
	CryptoManager.InitializationValues vals =
		new CryptoManager.InitializationValues
                    ("secmod.db", "key3.db", "cert7.db");
	CryptoManager.initialize(vals);

        try {
            System.out.println("Sleeping...");
            Thread.currentThread().sleep(5000);
            System.out.println("Awake.");
        } catch(Throwable t) { }
      

//	NSSInit.initialize("secmod.db", "key3.db", "cert7.db");
      }
      catch (KeyDatabaseException kdbe) {
	System.out.println("Couldn't open the key database");
	return;
      }
      catch (CertDatabaseException cdbe) {
	System.out.println("Couldn't open the certificate database");
	return;
      }
      catch (AlreadyInitializedException aie) {
	System.out.println("CryptoManager already initialized???");
	return;
      }
      catch (Exception e) {
	System.out.println("Exception occurred: "+e.getMessage());
	return;
      }
      
      /* enable all the SSL2 cipher suites */
      for (i = SSLSocket.SSL2_RC4_128_WITH_MD5;
	   i <= SSLSocket.SSL2_DES_192_EDE3_CBC_WITH_MD5; ++i) {
//	SSLSocket.setPermittedByPolicy(i, SSLSocket.SSL_ALLOWED);
        if( i != 0xFF05 ) {
            SSLSocket.setCipherPreference( i, true);
        }
      }
      
      /* enable all the SSL3 cipher suites */
      for (i = 0; cipherSuites[i] != 0;  ++i) {
//	SSLSocket.setPermittedByPolicy(cipherSuites[i], SSLSocket.SSL_ALLOWED);
	SSLSocket.setCipherPreference( cipherSuites[i], true);
      }
      
      SSLClient x = new SSLClient(System.out, "Stand alone Ver 0.01");
      x.run(true);
    }
  
}


class ClientHandshakeCB implements SSLHandshakeCompletedListener {
  SSLClient sc;
  
  ClientHandshakeCB(SSLClient sc) {
    this.sc = sc;
  }
  
  public void handshakeCompleted(SSLHandshakeCompletedEvent event) {
  try {
    sc.handshakeEventHappened = true;
    SSLSecurityStatus status = event.getStatus();
    sc.results.println("handshake happened\n" + status);
    System.out.println("Cert is " + status.getPeerCertificate());
  } catch(Exception e) {
        e.printStackTrace();
  }
  }
}

