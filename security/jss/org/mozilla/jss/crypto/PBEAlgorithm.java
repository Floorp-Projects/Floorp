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
import java.security.NoSuchAlgorithmException;

/**
 * Algorithms that can be used for generating symmetric keys from passwords.
 */
public class PBEAlgorithm extends KeyGenAlgorithm {

    private EncryptionAlgorithm encAlg;
    private int saltLength;

    protected PBEAlgorithm(int oidTag, String name, int validStrength,
            OBJECT_IDENTIFIER oid, EncryptionAlgorithm encAlg, int saltLength)
    {
        super(oidTag, name, new FixedKeyStrengthValidator(validStrength),
            oid, PBEKeyGenParams.class);
        this.encAlg = encAlg;
        this.saltLength = saltLength;
    }

    /**
     * Returns the EncryptionAlgorithm that should be used with keys
     * generated with this PBEAlgorithm. For example,
     *  <code>PBE_MD2_DES_CBC.getEncryptionAlg()</code> returns
     *  <code>EncryptionAlgorithm.DES_CBC</code>.
     */
    public EncryptionAlgorithm getEncryptionAlg() {
        return encAlg;
    }

    /**
     * Returns the number of bytes of salt that should be supplied when
     * generating keys with this algorithm.
     *
     * <p>PKCS #5 algorithms require exactly 8 bytes of salt.  PKCS #12
     *      algorithms take
     *      a variable length, but recommend that the salt length be at least
     *      as long as the output of the hash function.  For SHA-1, the output
     *      length is 20 bytes.
     */
    public int getSaltLength() {
        return saltLength;
    }

    ///////////////////////////////////////////////////////////////////////
    // OIDs
    ///////////////////////////////////////////////////////////////////////
    private static final OBJECT_IDENTIFIER PKCS5 = OBJECT_IDENTIFIER.PKCS5;
    private static final OBJECT_IDENTIFIER PKCS12_PBE =
            OBJECT_IDENTIFIER.PKCS12.subBranch(1);

    ///////////////////////////////////////////////////////////////////////
    // OID mapping
    ///////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////
    public static final PBEAlgorithm
    PBE_MD2_DES_CBC = new PBEAlgorithm(
        SEC_OID_PKCS5_PBE_WITH_MD2_AND_DES_CBC, "PBE/MD2/DES/CBC", 56,
            PKCS5.subBranch(1), EncryptionAlgorithm.DES_CBC, 8 );

    //////////////////////////////////////////////////////////////
    public static final PBEAlgorithm
    PBE_MD5_DES_CBC = new PBEAlgorithm(
        SEC_OID_PKCS5_PBE_WITH_MD5_AND_DES_CBC, "PBE/MD5/DES/CBC", 56,
            PKCS5.subBranch(3), EncryptionAlgorithm.DES_CBC, 8 );

    //////////////////////////////////////////////////////////////
    public static final PBEAlgorithm
    PBE_SHA1_DES_CBC = new PBEAlgorithm(
        SEC_OID_PKCS5_PBE_WITH_SHA1_AND_DES_CBC, "PBE/SHA1/DES/CBC", 56,
            PKCS5.subBranch(10), EncryptionAlgorithm.DES_CBC, 8 );

    //////////////////////////////////////////////////////////////
    public static final PBEAlgorithm
    PBE_SHA1_RC4_128 = new PBEAlgorithm(
        SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC4,
            "PBE/SHA1/RC4-128", 128, PKCS12_PBE.subBranch(1),
            EncryptionAlgorithm.RC4, 20 );

    //////////////////////////////////////////////////////////////
    public static final PBEAlgorithm
    PBE_SHA1_RC4_40 = new PBEAlgorithm(
        SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC4,
            "PBE/SHA1/RC4-40", 40, PKCS12_PBE.subBranch(2),
            EncryptionAlgorithm.RC4, 20 );

    //////////////////////////////////////////////////////////////
    public static final PBEAlgorithm
    PBE_SHA1_DES3_CBC = new PBEAlgorithm(
        SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC,
            "PBE/SHA1/DES3/CBC", 168, PKCS12_PBE.subBranch(3),
            EncryptionAlgorithm.DES3_CBC, 20 );

    //////////////////////////////////////////////////////////////
    public static final PBEAlgorithm
    PBE_SHA1_RC2_128_CBC = new PBEAlgorithm(
        SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_128_BIT_RC2_CBC,
            "PBE/SHA1/RC2-128", 128, PKCS12_PBE.subBranch(5),
            EncryptionAlgorithm.RC2_CBC, 20 );

    //////////////////////////////////////////////////////////////
    public static final PBEAlgorithm
    PBE_SHA1_RC2_40_CBC = new PBEAlgorithm(
        SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_40_BIT_RC2_CBC,
            "PBE/SHA1/RC2-40", 40, PKCS12_PBE.subBranch(6),
            EncryptionAlgorithm.RC2_CBC, 20 );
}
