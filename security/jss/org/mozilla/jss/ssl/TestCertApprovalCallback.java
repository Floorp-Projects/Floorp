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

package org.mozilla.jss.ssl;

import java.io.IOException;
import java.io.InterruptedIOException;
import java.net.*;
import java.util.*;
import org.mozilla.jss.crypto.*;

/**
 * This is a test implementation of the certificate approval callback which
 * gets invoked when the server presents a certificate which is not
 * trusted by the client
 */
public class TestCertApprovalCallback
   implements SSLCertificateApprovalCallback {

	public boolean approve(
					org.mozilla.jss.crypto.X509Certificate servercert,
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
			org.mozilla.jss.crypto.X509Certificate cert = item.getCert();
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


		if (trust_the_server_cert) {
			System.out.println("importing certificate.");
			try {
				InternalCertificate newcert = 
						org.mozilla.jss.CryptoManager.getInstance().
							importCertToPerm(servercert,"testnick");
				newcert.setSSLTrust(InternalCertificate.TRUSTED_PEER |
									InternalCertificate.VALID_PEER);
			} catch (Exception e) {
				System.out.println("thrown exception: "+e);
			}
		}

		
		/* allow the connection to continue.
			returning false here would abort the connection */
		return true;    
	}

}

