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

package com.netscape.jss.pkcs11;

import com.netscape.jss.crypto.*;
import com.netscape.jss.util.*;
import java.security.cert.CertificateEncodingException;
import java.util.Vector;

public final class PK11Store implements CryptoStore {

    ////////////////////////////////////////////////////////////
    // Private Keys
    ////////////////////////////////////////////////////////////
    public native void
    importEncryptedPrivateKey(  byte[] encodedKey,
                                Password password,
                                byte[] salt,
                                byte[] globalSalt,
                                PrivateKey.Type type    )
        throws InvalidKeyFormatException, TokenException;

    /**
     * Imports a raw private key into this token.
     *
     * @param key The private key.
     * @exception TokenException If the key cannot be imported to this token.
     * @exception KeyAlreadyImportedException If the key already on this token.
     */
    public native void
    importPrivateKey(  byte[] key,
                       PrivateKey.Type type       )
        throws TokenException,KeyAlreadyImportedException;



    /** 
     * @deprecated
     */
    public void
    importEncryptedPrivateKey(  byte[] encodedKey,
                                Password password,
                                byte[] salt,
                                byte[] globalSalt         )
                throws InvalidKeyFormatException, TokenException {

        importEncryptedPrivateKey(  encodedKey, password, salt, globalSalt,
                                    PrivateKey.RSA );
    }
                

    public native void
    importTemporaryEncryptedPrivateKey( byte[] encodedKey,
                                        Password password,
                                        byte[] salt,
                                        byte[] globalSalt,
                                        PrivateKey.Type type )
        throws InvalidKeyFormatException, TokenException;

    public synchronized PrivateKey[]
    getPrivateKeys() throws TokenException {
        Vector keys = new Vector();
        putKeysInVector(keys);
        PrivateKey[] array = new PrivateKey[keys.size()];
        keys.copyInto( (Object[]) array );
        return array;
    }
    protected native void putKeysInVector(Vector keys) throws TokenException;


    public native void deletePrivateKey(PrivateKey key)
        throws NoSuchItemOnTokenException, TokenException;

    ////////////////////////////////////////////////////////////
    // Certs
    ////////////////////////////////////////////////////////////

    public X509Certificate[]
    getCertificates() throws TokenException
    {
        Vector certs = new Vector();
        putCertsInVector(certs);
        X509Certificate[] array = new X509Certificate[certs.size()];
        certs.copyInto( (Object[]) array );
        return array;
    }
    protected native void putCertsInVector(Vector certs) throws TokenException;

	// Currently have to use PK11_DeleteTokenObject + PK11_FindObjectForCert
	// or maybe SEC_DeletePermCertificate.
    public native void deleteCert(X509Certificate cert)
        throws NoSuchItemOnTokenException, TokenException;

	////////////////////////////////////////////////////////////
	// Construction
	////////////////////////////////////////////////////////////
    protected boolean updated;
	public PK11Store(TokenProxy proxy) {
        Assert.assert(proxy!=null);
		this.storeProxy = proxy;
	}

	protected PK11Store() { }

	////////////////////////////////////////////////////////////
	// Private data
	////////////////////////////////////////////////////////////
	protected TokenProxy storeProxy;
}
