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
    public static final Type DiffieHellman = Type.DiffieHellman;

    /**
     * Returns the type (RSA or DSA) of this private key.
     */
    public Type getType();

    /**
     * Returns the unique ID of this key.  Unique IDs can be used to match
     * certificates to keys.
     *
     * @see org.mozilla.jss.crypto.TokenCertificate#getUniqueID
     * @deprecated This ID is based on an implementation that might change.
     *      If this functionality is required, it should be provided in
     *      another way, such as a function that directly matches a cert and
     *      key.
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
        private int pkcs11Type;

        private Type() { }

        private Type(OBJECT_IDENTIFIER oid, String name, int pkcs11Type) {
            this.oid = oid;
            this.name = name;
            Object old = oidMap.put(oid, this);
            this.pkcs11Type = pkcs11Type;
            Assert._assert( old == null );
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

        public int getPKCS11Type() {
            return pkcs11Type;
        }

        // OID for DiffieHellman, from RFC 2459 7.3.2.
        public static OBJECT_IDENTIFIER DH_OID =
            new OBJECT_IDENTIFIER( new long[] {1, 2, 840, 10046, 2, 1} );

        // From PKCS #11
        private static int CKK_RSA = 0x0;
        private static int CKK_DSA = 0x1;
        private static int CKK_DH = 0x2;
        
        public static final Type RSA = new Type(
                OBJECT_IDENTIFIER.PKCS1.subBranch(1), "RSA", CKK_RSA );
        public static final Type DSA = new Type(
                Algorithm.ANSI_X9_ALGORITHM.subBranch(1), "DSA", CKK_DSA); 
        public static final Type DiffieHellman = new Type(
                DH_OID, "DiffieHellman", CKK_DH );
                
    }
}
