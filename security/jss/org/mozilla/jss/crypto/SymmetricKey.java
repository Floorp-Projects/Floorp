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

import java.util.Hashtable;
import java.security.NoSuchAlgorithmException;

public interface SymmetricKey {

    public static final Type DES = Type.DES;
    public static final Type DES3 = Type.DES3;
    public static final Type RC4 = Type.RC4;
    public static final Type RC2 = Type.RC2;
    public static final Type SHA1_HMAC = Type.SHA1_HMAC;
    public Type getType();

    public CryptoToken getOwningToken();

    public int getStrength();
    public int getLength();

    public byte[] getKeyData() throws NotExtractableException;

    public static class NotExtractableException extends Exception { }

    String getAlgorithm();

    byte[] getEncoded();

    String getFormat();

    public final static class Type {
        // all names converted to lowercase for case insensitivity
        private static Hashtable nameMap = new Hashtable();

        private String name;
        private KeyGenAlgorithm keyGenAlg;
        private Type() { }
        private Type(String name, KeyGenAlgorithm keyGenAlg) {
            this.name = name;
            this.keyGenAlg = keyGenAlg;
            nameMap.put(name.toLowerCase(), this);
        }
        public static final Type DES = new Type("DES", KeyGenAlgorithm.DES);
        public static final Type DES3 =
            new Type("DESede", KeyGenAlgorithm.DES3);
        public static final Type DESede = DES3;
        public static final Type RC4 = new Type("RC4", KeyGenAlgorithm.RC4);
        public static final Type RC2 = new Type("RC2", null);
        public static final Type SHA1_HMAC = new Type("SHA1_HMAC",
            KeyGenAlgorithm.PBA_SHA1_HMAC);
        public static final Type AES = new Type("AES", KeyGenAlgorithm.AES);


        public String toString() {
            return name;
        }

        public KeyGenAlgorithm getKeyGenAlg() throws NoSuchAlgorithmException {
            if( keyGenAlg == null ) {
                throw new NoSuchAlgorithmException(name);
            }
            return keyGenAlg;
        }

        public static Type fromName(String name)
                throws NoSuchAlgorithmException
        {
            Object type = nameMap.get(name.toLowerCase());
            if( type == null ) {
                throw new NoSuchAlgorithmException();
            } else {
                return (Type) type;
            }
        }
    }

    /**
     * In PKCS #11, each key can be marked with the operations it will
     * be used to perform. Some tokens require that a key be marked for
     * an operation before the key can be used to perform that operation;
     * other tokens don't care.
     *
     * <p>When you unwrap a symmetric key, you must specify which one of these
     * operations it will be used to perform.
     */
    public final static class Usage {
        private Usage() { }
        private Usage(int val) { this.val = val;}
        private int val;

        public int getVal() { return val; }

        // these enums must match the JSS_symkeyUsage list in Algorithm.c
        public static final Usage ENCRYPT = new Usage(0);
        public static final Usage DECRYPT = new Usage(1);
        public static final Usage WRAP = new Usage(2);
        public static final Usage UNWRAP = new Usage(3);
        public static final Usage SIGN = new Usage(4);
        public static final Usage VERIFY = new Usage(5);
    }
}
