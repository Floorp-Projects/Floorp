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
import java.security.NoSuchAlgorithmException;

/**
 * Represents a cryptographic algorithm.
 * @see EncryptionAlgorithm
 * @see SignatureAlgorithm
 */
public class Algorithm {

    private Algorithm() { }

    /**
     * @param oidIndex Index of the oid that this algorithm represents.
     * @param name A String representation of the Algorithm.
     */
    protected Algorithm(int oidIndex, String name) {
        this.oidIndex = oidIndex;
        this.name = name;
    }

    /**
     * @param oidIndex Index of the oid that this algorithm represents.
     * @param name A String representation of the Algorithm.
     * @param oid The object identifier for this Algorithm.
     */
    protected Algorithm(int oidIndex, String name, OBJECT_IDENTIFIER oid) {
        this(oidIndex, name);
        this.oid = oid;
    }

    protected Algorithm(int oidIndex, String name, OBJECT_IDENTIFIER oid,
                        Class paramClass)
    {
        this(oidIndex, name, oid);
        if( paramClass == null ) {
            this.parameterClasses = new Class[0];
        } else {
            this.parameterClasses = new Class[1];
            this.parameterClasses[0] = paramClass;
        }
    }

    protected Algorithm(int oidIndex, String name, OBJECT_IDENTIFIER oid,
                        Class []paramClasses)
    {
        this(oidIndex, name, oid);
        if( paramClasses != null ) {
            this.parameterClasses = paramClasses;
        }
    }

    /**
     * Returns a String representation of the algorithm.
     */
    public String toString() {
        return name;
    }

    /**
     * Returns the object identifier for this algorithm.
     * @exception NoSuchAlgorithmException If no OID is registered for this
     *      algorithm.
     */
    public OBJECT_IDENTIFIER toOID() throws NoSuchAlgorithmException {
        if( oid == null ) {
            throw new NoSuchAlgorithmException();
        } else {
            return oid;
        }
    }

    /**
     * The type of parameter that this algorithm expects.  Returns
     *   <code>null</code> if this algorithm does not take any parameters.
     * If the algorithm can accept more than one type of parameter,
     *   this method returns only one of them. It is better to call
     *   <tt>getParameterClasses()</tt>.
     * @deprecated Call <tt>getParameterClasses()</tt> instead.
     */
    public Class getParameterClass() {
        if( parameterClasses.length == 0) {
            return null;
        } else {
            return parameterClasses[0];
        }
    }

    /**
     * The types of parameter that this algorithm expects.  Returns
     *   <code>null</code> if this algorithm does not take any parameters.
     */
    public Class[] getParameterClasses() {
        return (Class[]) parameterClasses.clone();
    }

    /**
     * Returns <tt>true</tt> if the given Object can be used as a parameter
     * for this algorithm.
     * <p>If <tt>null</tt> is passed in, this method will return <tt>true</tt>
     *      if this algorithm takes no parameters, and <tt>false</tt>
     *      if this algorithm does take parameters.
     */
    public boolean isValidParameterObject(Object o) {
        if( o == null ) {
            return (parameterClasses.length == 0);
        }
        if( parameterClasses.length == 0 ){
            return false;
        }
        Class c = o.getClass();
        for( int i = 0; i < parameterClasses.length; ++i) {
            if( c.equals( parameterClasses[i] ) ) {
                return true;
            }
        }
        return false;
    }

    /**
     * Index into the SECOidTag array in Algorithm.c.
     */
    protected int oidIndex;
    String name;
    protected OBJECT_IDENTIFIER oid;
    private Class[] parameterClasses=new Class[0];

    //////////////////////////////////////////////////////////////
    // Algorithm OIDs
    //////////////////////////////////////////////////////////////
    static final OBJECT_IDENTIFIER ANSI_X9_ALGORITHM = 
        new OBJECT_IDENTIFIER( new long[] { 1, 2, 840, 10040, 4 } );

    // Algorithm indices.  These must be kept in sync with the
    // algorithm array in Algorithm.c.
    protected static final short SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION=0;
    protected static final short SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION=1;
    protected static final short SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION=2;
    protected static final short SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST=3;
    protected static final short SEC_OID_PKCS1_RSA_ENCRYPTION=4;
    protected static final short CKM_RSA_PKCS_KEY_PAIR_GEN=5;
    protected static final short CKM_DSA_KEY_PAIR_GEN=6;
    protected static final short SEC_OID_ANSIX9_DSA_SIGNATURE=7;
    protected static final short SEC_OID_RC4=8;
    protected static final short SEC_OID_DES_ECB=9;
    protected static final short SEC_OID_DES_CBC=10;
    protected static final short CKM_DES_CBC_PAD=11;
    protected static final short CKM_DES3_ECB=12;
    protected static final short SEC_OID_DES_EDE3_CBC=13;
    protected static final short CKM_DES3_CBC_PAD=14;
    protected static final short CKM_DES_KEY_GEN=15;
    protected static final short CKM_DES3_KEY_GEN=16;
    protected static final short CKM_RC4_KEY_GEN=17;

    protected static final short SEC_OID_PKCS5_PBE_WITH_MD2_AND_DES_CBC=18;
    protected static final short SEC_OID_PKCS5_PBE_WITH_MD5_AND_DES_CBC=19;
    protected static final short SEC_OID_PKCS5_PBE_WITH_SHA1_AND_DES_CBC=20;
    protected static final short
        SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC4=21;
    protected static final short
        SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC4=22;
    protected static final short
        SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC=23;
    protected static final short SEC_OID_MD2=24;
    protected static final short SEC_OID_MD5=25;
    protected static final short SEC_OID_SHA1=26;
    protected static final short CKM_SHA_1_HMAC=27;
    protected static final short
        SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC=28;
    protected static final short
        SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC=29;
    protected static final short SEC_OID_RC2_CBC=30;
    protected static final short CKM_PBA_SHA1_WITH_SHA1_HMAC=31;

    // AES
    protected static final short CKM_AES_KEY_GEN=32;
    protected static final short CKM_AES_ECB=33;
    protected static final short CKM_AES_CBC=34;
    protected static final short CKM_AES_CBC_PAD=35;
    protected static final short CKM_RC2_CBC_PAD=36;
    protected static final short CKM_RC2_KEY_GEN=37;
    //FIPS 180-2
    protected static final short SEC_OID_SHA256=38;
    protected static final short SEC_OID_SHA384=39;
    protected static final short SEC_OID_SHA512=40;
    protected static final short SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION=41;
    protected static final short SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION=42;
    protected static final short SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION=43;

}
