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

package org.mozilla.jss.crypto;

import org.mozilla.jss.asn1.OBJECT_IDENTIFIER;
import java.util.Hashtable;
import org.mozilla.jss.util.Assert;
import java.security.NoSuchAlgorithmException;

/**
 * Private Keys used by JSS.  All the private keys handled by JSS are
 * of this type, which is a subtype of java.security.PrivateKey.
 */
public interface PrivateKey extends java.security.PrivateKey
{

    public static final Type RSA = Type.RSA;
    public static final Type DSA = Type.DSA;

    /**
     * Returns the type (RSA or DSA) of this private key.
     */
    public Type getType();

    /**
     * Returns the unique ID of this key.  Unique IDs can be used to match
     * certificates to keys.
     *
     * @see org.mozilla.jss.crypto.TokenCertificate#getUniqueID
     */
    public byte[] getUniqueID() throws TokenException;

    /**
     * Returns the size, in bits, of the modulus of an RSA key.
     * Returns -1 for other types of keys.
     */
    public int getStrength();

    /**
     * Returns the CryptoToken that owns this private key. Cryptographic
     * operations with this key may only be performed on the token that
     * owns the key.
     */
    public CryptoToken getOwningToken();

    public static final class Type {
        private OBJECT_IDENTIFIER oid;
        private String name;

        private Type() { }

        private Type(OBJECT_IDENTIFIER oid, String name) {
            this.oid = oid;
            this.name = name;
            Object old = oidMap.put(oid, this);
            Assert.assert( old == null );
        }

        private static Hashtable oidMap = new Hashtable();


        public static Type fromOID(OBJECT_IDENTIFIER oid)
            throws NoSuchAlgorithmException
        {
            Object obj = oidMap.get(oid);
            if( obj == null ) {
                throw new NoSuchAlgorithmException();
            }
            return (Type) obj;
        }

        /**
         * Returns a string representation of the algorithm, such as
         * "RSA" or "DSA".
         */
        public String toString() {
            return name;
        }

        public OBJECT_IDENTIFIER toOID() {
            return oid;
        }
        
        public static final Type RSA = new Type(
                OBJECT_IDENTIFIER.PKCS1.subBranch(1), "RSA" );
        public static final Type DSA = new Type(
                Algorithm.ANSI_X9_ALGORITHM.subBranch(1), "DSA" ); 
    }
}
