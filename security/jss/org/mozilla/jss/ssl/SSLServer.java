/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Security Services for Java.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// This program demonstrates SSL server-side support. It expects
// (HTTP) clients to connect to it, and will respond to HTTP
// GET requests 
//

// tabstops: 4

package org.mozilla.jss.ssl;

import java.io.*;
import java.util.*;
import org.mozilla.jss.*;

/**
 * Parameters supported by this socket test:
 * 
 * filename 	file to be read from https server (default: /index.html)
 * port 	port to connect to (default: 443)
 * clientauth 	do client-auth or not (default: no client-auth)
 *
 * The following parameters are used for regression testing, so
 * we can print success or failure of the test.
 * 
 * filesize 	size of file to be read
 * status 	security status of connection - this has to be an integer
 * 
 */

public class SSLServer 
{
    boolean     handshakeEventHappened = false;
    boolean     doClientAuth = false;
    Hashtable   args;
    PrintStream results;
    String      versionStr;

    String argNames[] = {
	"filename",
	"port",
	"filesize",
	"clientauth",
	"nickname"
    };

    String values[] = {
	"data1k.txt",           // filename
	"2000",			// port
	"1024",			// filesize
	"false",	        // request client auth 
	"SSLServer"             // nickname of cert to use
    };

    private static String htmlHeader = "SSL Server Tester";
    private static String htmlTail   = "\n";

    /* simple helper functions */
    private boolean isInvalid(String s) {
	return (s == null) || s.equals("");
    }

    private String getArgument(String key) {
	return (String) args.get(key);
    }

    String okay = "okay";
    String failed = "FAILED";

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


    public void run() 
    {
	try {
	    SSLServerSocket l;
	    SSLSocket       s;
	    int             port;
	    String          tmpStr;
	    String          portStr;
	    String          nickname;

	    results.println(htmlHeader);

	    portStr = getArgument("port");
	    if(isInvalid(portStr)) {
		port = 443;
	    } else {
		port = Integer.valueOf(portStr).intValue();
	    }
	    
	    results.println("here");
	    tmpStr = getArgument("clientauth");
	    if(!isInvalid(tmpStr)) {
		tmpStr       = tmpStr.toLowerCase();
		doClientAuth = !(tmpStr.equals("off") ||
				 tmpStr.equals("false") ||
				 tmpStr.equals("0"));
	    }

	    l = new SSLServerSocket(port);
	    results.println("Listening " + l.toString());

	    // select the server's cert/key
	    nickname = getArgument("nickname");
	    results.println("Getting Cert:"+nickname+"");
	    l.setServerCertNickname(nickname);

	    if (doClientAuth) {
	    	l.setNeedClientAuth(true);
	    }

	    // wait for and accept a connection.
            s = (SSLSocket)l.accept();
	    results.println("Accepted.");

	    handleConnection(s);
	    // handleConnection will close s, as appropriate.
	    s = null;

	    l.close();
	    l = null;

	} catch(Exception e) {
	    results.println("***** TEST FAILED *****");
	    e.printStackTrace(results);
	    results.println(
"If there is no stack trace, try disabling the JIT and trying again.");
	}
	results.println("END OF TEST");
    }

    public void handleConnection(SSLSocket s) throws Exception
    {
	SSLHandshakeCompletedListener listener = null;

	listener = new ServerHandshakeCB(this);
	s.addHandshakeCompletedListener(listener);

	results.println("Connected to " + s.toString());

	InputStream in         = s.getInputStream();
	byte[]      bytes      = new byte[4096];
	int         totalBytes = 0;
	int         numReads   = 0;

	// now try to read data from the SSL connection
	try {
	    boolean endFound = false;
	    while(! endFound && totalBytes < bytes.length) {
		results.println("Calling Read.");
		int n = in.read(bytes, totalBytes, 
				bytes.length - totalBytes);
		if(n == -1) {
		    results.println("EOF found.");
		    break;
		}

		if(n == 0) {
		    results.println("Zero bytes read?");
		    break;
		}
		numReads++;

		results.println("Read " + n + " bytes of data");
		totalBytes += n;
		for (int i = 0; i + 3 < bytes.length; ++i) {
		    if (bytes[i] == 0x0D &&
		        bytes[i+1] == 0x0A &&
		        bytes[i+2] == 0x0D &&
		        bytes[i+3] == 0x0A )
		    {
			results.println("Empty line found.");
		    	endFound = true;
			break;
		    }
		}
	    }
	} catch (IOException e) {
	    results.println(
		"IOException while reading from pipe?  Actually got " + 
		totalBytes + " bytes total");
	    e.printStackTrace(results);
	    results.println("");
	    throw e;
	} 

	results.println("Number of read() calls: " + numReads );
	results.println("Total bytes read:       " + totalBytes );
	String msg = null;

	if (totalBytes > 0) {
	    msg = new String(bytes, 0, totalBytes, "8859_1");
	    results.println("Request received:");
	    results.println(msg);
	    results.println("");
	}


	SSLSecurityStatus status = s.getStatus();
	results.println("Security status of session:");
	results.println(status.toString());

	results.println("Handshake callback event happened: " +
			((handshakeEventHappened) ? okay : failed));

	OutputStream            os  = s.getOutputStream();
	PrintOutputStreamWriter out = new PrintOutputStreamWriter(os);
	os = null;	// don't keep this reference lying around

	String reply =  "HTTP/1.0 200 OK\r\n" +
			"Server: Netscape-Enterprise/2.0a\r\n" +
			"Date: Tue, 01 Apr 1998 22:10:05 GMT\r\n" +
			"Content-type: text/plain\r\n" +
			"\r\n" +
			msg;
	out.println(reply);

	// Got here, so no exception thrown above.
	// Try to clean up.
	in.close();
	in = null;

	if (listener != null) {
	    s.removeHandshakeCompletedListener(listener);
	    listener = null;
	}

	out.close();
	out = null;

	s.close();
	s = null;
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

    public SSLServer( PrintStream ps, String verStr)
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

    public static void main(String argv[]) throws Exception
    {
	int i;

        System.out.println("SSLServer started\n");

	try {
		CryptoManager.initialize(".");
	}
	catch (KeyDatabaseException kdbe) {
          System.out.println("Couldn't open the key database\n");
	  return;
	}
	catch (CertDatabaseException cdbe) {
	  System.out.println("Couldn't open the certificate database");
	  return;
	}
    catch (org.mozilla.jss.crypto.AlreadyInitializedException aie) {
      System.out.println("CryptoManager already initialized???");
      return;
    }
    catch (java.security.GeneralSecurityException e) {
      System.out.println("General security exception while initializing");
      return;
    }
	  
	SSLServerSocket.configServerSessionIDCache(10, 0, 0, null);

	/* enable all the SSL2 cipher suites */
	for (i = SSLSocket.SSL2_RC4_128_WITH_MD5;
	     i <= SSLSocket.SSL2_DES_192_EDE3_CBC_WITH_MD5; ++i) {
//	    SSLSocket.setPermittedByPolicy(i, SSLSocket.SSL_ALLOWED);
	    SSLSocket.setCipherPreferenceDefault( i, true);
	}

	/* enable all the SSL3 cipher suites */
	for (i = 0; cipherSuites[i] != 0;  ++i) {
//	    SSLSocket.setPermittedByPolicy(cipherSuites[i], SSLSocket.SSL_ALLOWED);
	    SSLSocket.setCipherPreferenceDefault( cipherSuites[i], true);
	}

	SSLServer x = new SSLServer(System.out, "Stand alone Ver 0.01");
	x.run();
    }

}


class ServerHandshakeCB implements SSLHandshakeCompletedListener {
    SSLServer sc;

    ServerHandshakeCB(SSLServer sc) {
	this.sc = sc;
    }

    public void handshakeCompleted(SSLHandshakeCompletedEvent event) {
	sc.handshakeEventHappened = true;
	sc.results.println("handshake happened");
    }
}

