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

package org.mozilla.jss.pkcs11;

import org.mozilla.jss.crypto.*;
import org.mozilla.jss.util.*;
import java.security.*;
import java.security.cert.*;
import java.util.*;
import java.math.BigInteger;

public class PK11Cert implements org.mozilla.jss.crypto.X509Certificate {

	public native byte[] getEncoded() throws CertificateEncodingException;

    //public native byte[] getUniqueID();

    public native String getNickname();

    /**
     * A class that implements Principal with a String.
     */
    protected static class StringPrincipal implements Principal {
        public StringPrincipal(String str) {
            this.str = str;
        }

        public boolean
        equals(Object other) {
            if( ! (other instanceof StringPrincipal) ) {
                return false;
            }
            return getName().equals( ((StringPrincipal)other).getName() );
        }

        public String getName() {
            return str;
        }
        public int hashCode() {
            return str.hashCode();
        }

        public String toString() {
            return str;
        }
        protected String str;
    }

    public Principal
    getSubjectDN() {
        return new StringPrincipal( getSubjectDNString() );
    }

    public Principal
    getIssuerDN() {
        return new StringPrincipal( getIssuerDNString() );
    }

    public BigInteger
    getSerialNumber() {
        return new BigInteger( getSerialNumberByteArray() );
    }
    protected native byte[] getSerialNumberByteArray();

    protected native String getSubjectDNString();

    protected native String getIssuerDNString();

	public native java.security.PublicKey getPublicKey();

	public native int getVersion();

    
    ///////////////////////////////////////////////////////////////////////
    // PKCS #11 Cert stuff. Must only be called on certs that have
    // an associated slot.
    ///////////////////////////////////////////////////////////////////////
    protected native byte[] getUniqueID();

    protected native CryptoToken getOwningToken();

    ///////////////////////////////////////////////////////////////////////
    // Trust Management.  Must only be called on certs that live in the
    // internal database.
    ///////////////////////////////////////////////////////////////////////
    /**
     * Sets the trust flags for this cert.
     *
     * @param type SSL, EMAIL, or OBJECT_SIGNING.
     * @param trust The trust flags for this type of trust.
     */
    protected native void setTrust(int type, int trust);

    /**
     * Gets the trust flags for this cert.
     *
     * @param type SSL, EMAIL, or OBJECT_SIGNING.
     * @return The trust flags for this type of trust.
     */
    protected native int getTrust(int type);

	/////////////////////////////////////////////////////////////
	// Construction
	/////////////////////////////////////////////////////////////
	//PK11Cert(CertProxy proxy) {
    //    Assert._assert(proxy!=null);
	//	this.certProxy = proxy;
	//}

	PK11Cert(byte[] pointer) {
        Assert._assert(pointer!=null);
		certProxy = new CertProxy(pointer);
	}

	/////////////////////////////////////////////////////////////
	// private data
	/////////////////////////////////////////////////////////////
	protected CertProxy certProxy;
}

class CertProxy extends org.mozilla.jss.util.NativeProxy {

    public CertProxy(byte[] pointer) {
        super(pointer);
    }

    protected native void releaseNativeResources();

    protected void finalize() throws Throwable {
		Debug.trace(Debug.OBNOXIOUS, "finalizing a certificate");
        super.finalize();
    }
}
