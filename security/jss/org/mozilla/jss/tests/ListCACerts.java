package org.mozilla.jss.tests;

import org.mozilla.jss.CryptoManager;
import org.mozilla.jss.crypto.*;

public class ListCACerts {

    public static void main(String args[]) throws Exception {
	if( args.length != 1 ) {
            System.out.println(
             "Usage: java org.mozilla.jss.tests.ListCACerts <dbdir>");
	     System.exit(1);
	 }
	try {
	    CryptoManager.initialize(args[0]);
	    CryptoManager cm = CryptoManager.getInstance();

	    X509Certificate[] certs = cm.getCACerts();

	    for(int i=0; i < certs.length; ++i ) {
		System.out.println(certs[i].getSubjectDN().toString());
		InternalCertificate ic = (InternalCertificate) certs[i];
		System.out.println("SSL: " + ic.getSSLTrust() + ", Email: " +
		    ic.getEmailTrust() + ", Object Signing: " +
		    ic.getObjectSigningTrust());
	    }
	} catch(Throwable e) {
	      e.printStackTrace();
	      System.exit(1);
	}
	System.exit(0);
    }
}
