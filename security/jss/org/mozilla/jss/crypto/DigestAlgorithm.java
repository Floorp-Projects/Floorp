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

import java.util.Hashtable;
import java.security.NoSuchAlgorithmException;
import org.mozilla.jss.asn1.*;

public class DigestAlgorithm extends Algorithm {

    // The size in bytes of the output of this hash.
    private int outputSize;

    protected DigestAlgorithm(int oidIndex, String name,
            OBJECT_IDENTIFIER oid, int outputSize)
    {
        super(oidIndex, name, oid);

        this.outputSize = outputSize;

        // only store the first algorithm for a given oid.  More than one
        // alg might share the same oid, such as from child classes.
        if( oid != null && oidMap.get(oid)==null ) {
            oidMap.put(oid, this);
        }
    }

    ///////////////////////////////////////////////////////////////////////
    // OID mapping
    ///////////////////////////////////////////////////////////////////////
    private static Hashtable oidMap = new Hashtable();

    public static DigestAlgorithm fromOID(OBJECT_IDENTIFIER oid)
        throws NoSuchAlgorithmException
    {
        Object alg = oidMap.get(oid);
        if( alg == null ) {
            throw new NoSuchAlgorithmException();
        } else {
            return (DigestAlgorithm) alg;
        }
    }

    /**
     * Returns the output size in bytes for this algorithm.
     */
    public int getOutputSize() {
        return outputSize;
    }

    /**
     * The MD2 digest algorithm, from RSA.
     */
    public static final DigestAlgorithm MD2 = new DigestAlgorithm
        (SEC_OID_MD2, "MD2", OBJECT_IDENTIFIER.RSA_DIGEST.subBranch(2), 16 );

    /**
     * The MD5 digest algorithm, from RSA.
     */
    public static final DigestAlgorithm MD5 = new DigestAlgorithm
        (SEC_OID_MD5, "MD5", OBJECT_IDENTIFIER.RSA_DIGEST.subBranch(5), 16 );

    /**
     * The SHA-1 digest algorithm, from Uncle Sam.
     */
    public static final DigestAlgorithm SHA1 = new DigestAlgorithm
        (SEC_OID_SHA1, "SHA-1", OBJECT_IDENTIFIER.ALGORITHM.subBranch(26), 20);

    /*
    * The SHA-256 digest Algorithm from FIPS 180-2  
    */
    public static final DigestAlgorithm SHA256 = new DigestAlgorithm
        (SEC_OID_SHA256, "SHA-256", OBJECT_IDENTIFIER.HASH_ALGORITHM.subBranch(1), 32);

    /*
    * The SHA-384 digest Algorithm from FIPS 180-2  
    */
    public static final DigestAlgorithm SHA384 = new DigestAlgorithm
        (SEC_OID_SHA384, "SHA-384", OBJECT_IDENTIFIER.HASH_ALGORITHM.subBranch(2), 48);

    /*
    * The SHA-512 digest Algorithm from FIPS 180-2  
    */
    public static final DigestAlgorithm SHA512 = new DigestAlgorithm
        (SEC_OID_SHA512, "SHA-512", OBJECT_IDENTIFIER.HASH_ALGORITHM.subBranch(3), 64);

}
