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
import org.mozilla.jss.crypto.X509Certificate;

/**
 * This class represents the known state of an SSL connection: what cipher
 * is being used, how secure it is, and who's on the other end.
 */
public class SSLSecurityStatus {
    int status;
    String cipher;
    int sessionKeySize;
    int sessionSecretSize;
    String issuer;
    String subject;
    String serialNumber;
    X509Certificate certificate; // Certificate may be null if client does not present certificate

    final public int STATUS_NOOPT    = -1;
    final public int STATUS_OFF      = 0;
    final public int STATUS_ON_HIGH  = 1;
    final public int STATUS_ON_LOW   = 2;
    final public int STATUS_FORTEZZA = 3;

	/**
	 * This constructor is called from the native SSL code
	 * It's not necessary for you to call this.
 	 */
    public SSLSecurityStatus(int status, String cipher,
			     int sessionKeySize, int sessionSecretSize,
			     String issuer, String subject,
			     String serialNumber, X509Certificate certificate) {
	String noCert = "no certificate";
	this.status = status;
	this.cipher = cipher;
	this.sessionKeySize = sessionKeySize;
	this.sessionSecretSize = sessionSecretSize;
        this.certificate = certificate;
	
	if(noCert.equals(issuer))
	    this.issuer = null;
	else
	    this.issuer = issuer;
	    
	if(noCert.equals(subject))
	   this.subject = null;
	else
	   this.subject = subject;
	   
	this.serialNumber = serialNumber;
    }

    /**
     * Query if security is enabled on this socket.
     */
    public boolean isSecurityOn() {
	return status > 0;
    }

    /**
     * Get exact security status of socket.
     */
    public int getSecurityStatus() {
	return status;
    }

    /**
     * Query which cipher is being used in this session.
     */
    public String getCipher() {
	return cipher;
    }

    /**
     * Query how many bits long the session key is.  More bits are better.
     */
    public int getSessionKeySize() {
	return sessionKeySize;
    }

    /**
     * To satisfy export restrictions, some of the session key may
     * be revealed. This function tells you how many bits are
     * actually secret.
     */
    public int getSessionSecretSize() {
	return sessionSecretSize;
    }

    /**
     * Get the distinguished name of the remote certificate's issuer
     */
    public String getRemoteIssuer() {
	return issuer;
    }

    /**
     * Get the distinguished name of the subject of the remote certificate
     */
    public String getRemoteSubject() {
	return subject;
    }

    /**
     * Get the serial number of the remote certificate
     */
    public String getSerialNumber() {
	return serialNumber;
    }

    /**
      * Retrieve certificate presented by the other other end
      * of the socket <p>Not Supported in NSS 2.0 Beta release.
      * <p> Can be null if peer did not present a certificate.
      */
    public X509Certificate getPeerCertificate() {
        return certificate;
    }

    /**
     * Get a pretty string to show to a user, summarizing the contents
     * of this object
     */
    public String toString() {
	String statusString;
	switch(status) {
	case STATUS_NOOPT:
	    statusString = "NOOPT";
	    break;
	case STATUS_OFF:
	    statusString = "OFF";
	    break;
	case STATUS_ON_HIGH:
	    statusString = "ON HIGH";
	    break;
	case STATUS_ON_LOW:
	    statusString = "ON LOW";
	    break;
	case STATUS_FORTEZZA:
	    statusString = "FORTEZZA";
	    break;
	default:
	    statusString = "unknown";
	    break;

	}

	return "Status: " + statusString + "\n" +
	    "Cipher: " + cipher + "\n" +
	    "Session key size: " + sessionKeySize + "\n" +
	    "Session secret size: " + sessionSecretSize + "\n" +
	    "Issuer: " + issuer + "\n" +
	    "Subject: " + subject + "\n" +
	    "Serial number: " + serialNumber + "\n";
    }
}
