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
/*
 * SSLSecurityStatus.java
 * 
 */

package org.mozilla.jss.ssl;

import org.mozilla.jss.pkcs11.*;

import java.io.IOException;
import java.io.InterruptedIOException;
import java.util.*;
import java.net.*;
import org.mozilla.jss.crypto.X509Certificate;

/**
 * This interface is what you should implement if you want to
 * be able to decide whether or not you want to approve the peer's cert,
 * instead of having NSS do that.
 */
public interface SSLCertificateApprovalCallback {

	/**
	 * This method is called when the server sends it's certificate to
     * the client. 
     *
     * The 'status' argument passed to this method is constructed by
	 * NSS. It's a list of things 'wrong' with the certificate (which
	 * you can see by calling the status.getReasons() method. So,
	 * if there are problems regarding validity or trust of any of the
	 * certificates in the chain, you can present this info to the user.
	 *
	 * If there are no items in the Enumeration returned by getReasons(),
	 * you can assume that the certificate is trustworthy, and return
	 * true, or you can continue to make further tests of your own
	 * to determine trustworthiness.
	 *
	 * @param  cert the peer's server certificate
	 * @param  status the ValidityStatus object containing a list
     *            of all the problems with the cert
	 *
	 * @return <b>true</b> allow the connection to continue<br>
	 *         <b>false</b> terminate the connection (Expect an IOException
	 *              on the outstanding read()/write() on the socket)
	*/
	public boolean approve(org.mozilla.jss.crypto.X509Certificate cert,
			ValidityStatus status);

	/**
	 *  This class holds details about the errors for each cert in
     *  the chain that the server presented
	 *
	 *  To use this class, getReasons(), then iterate over the enumeration
     */

class ValidityStatus {

	public static final int  EXPIRED_CERTIFICATE    = -8192 + 11;
	public static final int  REVOKED_CERTIFICATE    = -8192 + 12;
	public static final int  INADEQUATE_KEY_USAGE   = -8192 + 90;
	public static final int  INADEQUATE_CERT_TYPE   = -8192 + 91;
	public static final int  UNTRUSTED_CERT         = -8192 + 21;
	public static final int  CERT_STATUS_SERVER_ERROR = -8192 +115;
	public static final int  UNKNOWN_ISSUER         = -8192 + 13;
	public static final int  UNTRUSTED_ISSUER       = -8192 + 20;
	public static final int  CERT_NOT_IN_NAME_SPACE = -8192 + 112;
	public static final int  CA_CERT_INVALID        = -8192 + 36;
	public static final int  PATH_LEN_CONSTRAINT_INVALID  = -8192 + 37;
	public static final int  BAD_KEY                = -8192 + 14;
	public static final int  BAD_SIGNATURE          = -8192 + 10;
	public static final int  EXPIRED_ISSUER_CERTIFICATE = -8192 +30;
	public static final int  INVALID_TIME                = -8192 + 8;
	public static final int  UNKNOWN_SIGNER              = -8192 + 116;
	public static final int  SEC_ERROR_CRL_EXPIRED       = -8192 + 31;
	public static final int  SEC_ERROR_CRL_BAD_SIGNATURE = -8192 + 32;
	public static final int  SEC_ERROR_CRL_INVALID       = -8192 + 33;
	public static final int  CERT_BAD_ACCESS_LOCATION    = -8192 + 117;
	public static final int  OCSP_UNKNOWN_RESPONSE_TYPE  = -8192 + 118;
	public static final int  OCSP_BAD_HTTP_RESPONSE      = -8192 + 119;
	public static final int  OCSP_MALFORMED_REQUEST      = -8192 + 120;
	public static final int  OCSP_SERVER_ERROR           = -8192 + 121;
	public static final int  OCSP_TRY_SERVER_LATER       = -8192 + 122;
	public static final int  OCSP_REQUEST_NEEDS_SIG      = -8192 + 123;
	public static final int  OCSP_UNAUTHORIZED_REQUEST   = -8192 + 124;
	public static final int  OCSP_UNKNOWN_RESPONSE_STATUS= -8192 + 125;
	public static final int  OCSP_UNKNOWN_CERT           = -8192 + 126;
	public static final int  OCSP_NOT_ENABLED            = -8192 + 127;
	public static final int  OCSP_NO_DEFAULT_RESPONDER   = -8192 + 128;
	public static final int  OCSP_MALFORMED_RESPONSE     = -8192 + 129;
	public static final int  OCSP_UNAUTHORIZED_RESPONSE  = -8192 + 130;
	public static final int  OCSP_FUTURE_RESPONSE        = -8192 + 131;
	public static final int  OCSP_OLD_RESPONSE           = -8192 + 132;

	/** this indicates common-name mismatch */
	public static final int  BAD_CERT_DOMAIN        = -12288 + 12;

	private Vector reasons = new Vector();

	/**
	 * add a new failure reason to this enumeration. This is called from the
	 * native code callback when it does a verify on the cert chain
	 * 
	 * @param newReason sslerr.h error code - see constants defined above;
	 * @param cert      a reference to the cert - so you can see the subject name, etc
     * @param depth     the index of this cert in the chain. 0 is the server cert.
     */

	public void addReason(int newReason,
							PK11Cert cert,
							int depth) {
		ValidityItem status = new ValidityItem(newReason,cert,depth);
		reasons.addElement(status);
	}

	/**
	 * returns an enumeration. The elements in the enumeration are
	 * all of type 'ValidityItem'
	 */
	public Enumeration getReasons() {
		return reasons.elements();
	}
}


class ValidityItem {
	private int reason;
	private int depth;
	private PK11Cert cert;

	public ValidityItem(int reason, 
							PK11Cert cert,
							int depth) {
		this.reason = reason;
		this.cert = cert;
		this.depth = depth;
	}

	/**
	 * @return the NSS error code which caused the error - see the list of
	 *     error codes above for those which could be returned
	 */
	public int getReason() {
		return reason;
	}

	/**
	 * @return the index into the cert chain of the certificate which caused
     *          this error. In a chain 5-certs long,  0 is the server-cert.
     *          1,2,3 would be the intermediates, and 4 would be the root
	 */
	public int getDepth() {
		return depth;
	}

	/**
	 * @return the certificate associated with this error. You can use 
	 *     the X509Certificate functions to get details such as issuer/subject
	 *     name, serial number, etc.
	 */
	public PK11Cert getCert() {
		return cert;
	}

}
	


}
