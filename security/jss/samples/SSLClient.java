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

import com.netscape.jss.ssl.*;

import java.io.*;
import java.util.*;
import java.net.*;
import com.netscape.jss.*;
import com.netscape.jss.crypto.AlreadyInitializedException;

/**
 * Parameters supported by this socket test:
 *   SSLClient <host=hostname> <port=portnum> <cert=clientcertnickname>
 *
 * 
 */

public class SSLClient implements 
  SSLCertificateApprovalCallback, SSLClientCertificateSelectionCallback
{
    boolean     handshakeEventHappened = false;
    boolean     doClientAuth = false;
    PrintStream results;

	Hashtable args;   // command line arguments are store here

	public static Hashtable initArgs() {
		Hashtable args = new Hashtable();

		args.put("hostname","localhost");
		args.put("port","10443");
		return args;
	}

	public String getArgument(String key) {
		return (String)args.get(key);
	}
		

  
  public void run() 
    {
      try {
		SSLHandshakeCompletedListener listener = null;
	
		SSLSocket s;
		String    hostname;
		int       port;

		String msg = "GET /index.html";

		String portstr = getArgument("port");
		if (portstr == null) {
			port = 443;
		}
		else {
  			port = Integer.valueOf(portstr).intValue();
		}

		/* the argument 'cert' specifies the nickname of
			a client-auth-cert to use if asked by the server */

		String nickname = getArgument("cert");
	
		hostname = getArgument("hostname");

		results.println("Connecting to " + hostname +
						" on port " + port );
	
		s = new SSLSocket(InetAddress.getByName(hostname),
						port,
						null,   /* local addr */
						0,      /* local port */
						true,   /* stream     */
						this,   /* approvalCallback */
						this    /* certSelectionCallback */
					);
						
		results.println("Connected.");
	
		// If you pass null for the certSelectionCallback argument
		// above, you can provide the name of the client-auth-
		// cert ahead of time, by using the setClientCertNickname
		// API 
		// select the cert for client auth
	
		// s.setClientCertNickname("cert");
	
	
        // Setup a handshake callback. This listener will get invoked
        // When the SSL handshake is completed on this socket.
	
		listener = new ClientHandshakeCB(this);
		s.addHandshakeCompletedListener(listener);
	
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
	    
				if (n == 0) {
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
			results.println("IOException while reading from pipe?  "+
							"Actually got "+ totalBytes + " bytes total");
  			e.printStackTrace(results);
  			results.println("");
  			throw e;
		} finally {
  			results.println("Last 30 bytes: " + lastBytes);
  			results.println("Number of read() calls: " + numReads );
			results.println("\nDiagnostics");

  
			results.println("Total bytes read: " + totalBytes );
			results.println("Security status of session:");
			SSLSecurityStatus status = s.getStatus();
			results.println(status.toString());

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


/* CONSTRUCTOR */
  
  public SSLClient( PrintStream ps, String verStr, Hashtable myargs)
	   throws Exception
    {

	  this.args = myargs;
	  this.results = ps;
      int i;

	  String ocsp = (String)myargs.get("ocsp_on");
	  boolean ocsp_on = false;
	  if (ocsp != null) {
	  	if (ocsp.equals("on") ) {
			ocsp_on = true;
	  	 }
	  	else if (ocsp.equals("off")) {
	 		ocsp_on = false;
	  	}
	  	else {
			throw new Exception("Error: invalid argument '"+
						ocsp+ "' must be 'on' or 'off'");
	  	}
	  }
			

      try {
		CryptoManager.InitializationValues vals =
			new CryptoManager.InitializationValues
                    ("secmod.db", "key3.db", "cert7.db");
		vals.ocspCheckingEnabled = ocsp_on;
		vals.ocspResponderURL    = (String)myargs.get("ocsp_url");
		vals.ocspResponderCertNickname = (String)myargs.get(
											"ocsp_nickname");
		CryptoManager.initialize(vals);
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
      
      /* enable all the SSL2 cipher suites */
      for (i = SSLSocket.SSL2_RC4_128_WITH_MD5;
	   	i <= SSLSocket.SSL2_DES_192_EDE3_CBC_WITH_MD5; ++i) {
			SSLSocket.setCipherPreference( i, true);
      }
      
      /* enable all the SSL3 cipher suites */
      for (i = 0; cipherSuites[i] != 0;  ++i) {
			SSLSocket.setCipherPreference( cipherSuites[i], true);
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

		Hashtable myargs = initArgs();
		int i;

		for (i=0;i<argv.length;i++) {
			int index = argv[i].indexOf("=");
			if (index == -1) {
				exitError("Invalid Argument: "+argv[i]);
			}
			String key   = argv[i].substring(0,index);
			String value = argv[i].substring(index+1);
			if (value== null) {
				exitError("Invalid value for argument: "+argv[i]);
			}
			myargs.put(key,value);
		}
      
		try { 
			SSLClient x = new SSLClient(System.out, "Stand alone Ver 0.1",myargs);
			x.run();
		} catch (Exception e) {
			e.printStackTrace();
		}
    }
  


  public static void exitError(String x) {
	System.err.println("Error: "+x);
	System.exit(1);
  }


/**
 * SSLCertificateApprovalCallback.approve()
 *
 * This is a test implementation of the certificate approval callback which
 * gets invoked when the server presents a certificate which is not
 * trusted by the client
 */

	public boolean approve(
					com.netscape.jss.crypto.X509Certificate servercert,
					SSLCertificateApprovalCallback.ValidityStatus status) {

		SSLCertificateApprovalCallback.ValidityItem item;

		System.out.println("in TestCertApprovalCallback.approve()");

		/* dump out server cert details */

		System.out.println("Peer cert details: "+
				"\n     subject: "+servercert.getSubjectDN().toString()+
				"\n     issuer:  "+servercert.getIssuerDN().toString()+
				"\n     serial:  "+servercert.getSerialNumber().toString()
				);

		/* iterate through all the problems */

		boolean trust_the_server_cert=false;

		Enumeration errors = status.getReasons();
		int i=0;
		while (errors.hasMoreElements()) {
			i++;
			item = (SSLCertificateApprovalCallback.ValidityItem) errors.nextElement();
			System.out.println("item "+i+
					" reason="+item.getReason()+
					" depth="+item.getDepth());
			com.netscape.jss.crypto.X509Certificate cert = item.getCert();
			if (item.getReason() == 
				SSLCertificateApprovalCallback.ValidityStatus.UNTRUSTED_ISSUER) {
				trust_the_server_cert = true;
			}
				
			System.out.println(" cert details: "+
				"\n     subject: "+cert.getSubjectDN().toString()+
				"\n     issuer:  "+cert.getIssuerDN().toString()+
				"\n     serial:  "+cert.getSerialNumber().toString()
				);
		}


/* - importCertToPerm is in JSS 2.1 only 
		if (trust_the_server_cert) {
			System.out.println("importing certificate.");
			try {
				InternalCertificate newcert = 
						com.netscape.jss.CryptoManager.getInstance().
							importCertToPerm(servercert,"testnick");
				newcert.setSSLTrust(InternalCertificate.TRUSTED_PEER |
									InternalCertificate.VALID_PEER);
			} catch (Exception e) {
				System.out.println("thrown exception: "+e);
			}
		}
*/

		
		/* allow the connection to continue.
			returning false here would abort the connection */
		return true;    
	}





/**
 * This interface is what you should implement if you want to
 * be able to decide whether or not you want to approve the peer's cert,
 * instead of having NSS do that.
 */

	/**
	 *  this method will be called form the native callback code
	 *  when a certificate is requested. You must return a String
	 *  which is the nickname of the certificate you wish to present.
	 *
	 *  @param nicknames A Vector of Strings. These strings are an
	 *    aid to the user to select the correct nickname. This list is
	 *    made from the list of all certs which are valid, match the
	 *    CA's trusted by the server, and which you have the private
	 *    key of. If nicknames.length is 0, you should present an
	 *    error to the user saying 'you do not have any unexpired
	 *    certificates'.
	 *  @return You must return the nickname of the certificate you
	 *    wish to use. You can return null if you do not wish to send
     *    a certificate.
	 */
	public String select(Vector nicknames) {
		Enumeration e = nicknames.elements();
		String s="",first=null;

		System.out.println("in TestClientCertificateSelectionCallback.select()  "+s);
		while (e.hasMoreElements()) {
			s = (String)e.nextElement();
			if (first == null) {
				first = s;
			}
			System.out.println("  "+s);
		}
		return first;

	}



}

class ClientHandshakeCB implements SSLHandshakeCompletedListener {
  SSLClient sc;
  
  ClientHandshakeCB(SSLClient sc) {
    this.sc = sc;
  }
  
  public void handshakeCompleted(SSLHandshakeCompletedEvent event) {
    sc.handshakeEventHappened = true;
    sc.results.println("handshake happened");
  }
}

class PrintOutputStreamWriter
    extends java.io.OutputStreamWriter
{

    public PrintOutputStreamWriter(OutputStream out)
    {
        super(out);
    }

    public void print(String x)
    throws  java.io.IOException
    {
        write(x, 0, x.length());
    }

    public void println(String x)
    throws  java.io.IOException
    {
    	String line = x + "\n";
    	write(line, 0, line.length());
    	flush();
    }
}


