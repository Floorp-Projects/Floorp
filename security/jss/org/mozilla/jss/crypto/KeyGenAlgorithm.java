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
import java.security.NoSuchAlgorithmException;

/**
 * Algorithms that can be used for generating symmetric keys.
 */
public class KeyGenAlgorithm extends Algorithm {

    protected KeyGenAlgorithm(int oidTag, String name, int validStrength,
            OBJECT_IDENTIFIER oid, Class paramClass)
    {
        super(oidTag, name, oid, paramClass);
        this.validStrength = validStrength;
        if(oid!=null) {
            oidMap.put(oid, this);
        }
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
    private static Hashtable oidMap = new Hashtable();

    public static KeyGenAlgorithm fromOID(OBJECT_IDENTIFIER oid)
        throws NoSuchAlgorithmException
    {
        Object alg = oidMap.get(oid);
        if( alg == null ) {
            throw new NoSuchAlgorithmException(oid.toString());
        } else {
            return (KeyGenAlgorithm) alg;
        }
    }

    // The valid strength (key size in bits) for keys of this algorithm.
    // A value of -1 means all strengths are valid (such as for RC4).
    private int validStrength;

    /**
     * Returns <code>true</code> if the given strength is valid for this
     * key generation algorithm. Note that PBE algorithms require
     * PBEParameterSpecs rather than strengths.  It is the responsibility
     * of the caller to verify this.
     */
    public boolean isValidStrength(int strength) {
        if( validStrength == -1 ) {
            return true;
        } else {
            return strength == validStrength;
        }
    }

    //////////////////////////////////////////////////////////////
    public static final KeyGenAlgorithm
    DES = new KeyGenAlgorithm(CKM_DES_KEY_GEN, "DES", 56, null, null);

    //////////////////////////////////////////////////////////////
    public static final KeyGenAlgorithm
    DES3 = new KeyGenAlgorithm(CKM_DES3_KEY_GEN, "DES3", 168, null, null);

    //////////////////////////////////////////////////////////////
    public static final KeyGenAlgorithm
    RC4 = new KeyGenAlgorithm(CKM_RC4_KEY_GEN, "RC4", -1, null, null);

    //////////////////////////////////////////////////////////////
    public static final KeyGenAlgorithm
    PBA_SHA1_HMAC = new KeyGenAlgorithm(
        CKM_PBA_SHA1_WITH_SHA1_HMAC,
            "PBA/SHA1/HMAC", 160, null, PBEKeyGenParams.class );
}
