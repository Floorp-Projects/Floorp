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
 * Algorithms that can be used for signing.
 */
public class SignatureAlgorithm extends Algorithm {

    private static Hashtable oidMap = new Hashtable();

    protected SignatureAlgorithm(int oidIndex, String name,
        SignatureAlgorithm signingAlg, DigestAlgorithm digestAlg,
        OBJECT_IDENTIFIER oid)
    {
        super(oidIndex, name, oid);
        if(signingAlg == null) {
            this.signingAlg = this;
        } else {
            this.signingAlg = signingAlg;
        }
        this.digestAlg = digestAlg;
        oidMap.put(oid, this);
    }

    /**
     * Looks up the signature algorithm with the given OID.
     * @exception NoSuchAlgorithmException If no algorithm is found with this
     *      OID.
     */
    public static SignatureAlgorithm fromOID(OBJECT_IDENTIFIER oid)
        throws NoSuchAlgorithmException
    {
        Object alg = oidMap.get(oid);  
        if( alg == null ) {
            throw new NoSuchAlgorithmException();
        }
        return (SignatureAlgorithm) alg;
    }

    /**
     * The raw encryption portion of the signature algorithm. For example, 
     * SignatureAlgorithm.RSASignatureWithMD2Digest.getSigningAlg ==
     * SignatureAlgorithm.RSASignature.
     */
    public Algorithm getSigningAlg() {
        return signingAlg;
    }
    public SignatureAlgorithm getRawAlg() {
        return signingAlg;
    }
    private SignatureAlgorithm signingAlg;

    /**
     * The digest portion of the signature algorithm.
     */
    public DigestAlgorithm getDigestAlg() throws NoSuchAlgorithmException {
        if( digestAlg == null ) {
            throw new NoSuchAlgorithmException();
        }
        return digestAlg;
    }
    private DigestAlgorithm digestAlg;

    //////////////////////////////////////////////////////////////////////
    // Signature Algorithms
    //////////////////////////////////////////////////////////////////////

    /**********************************************************************
     * Raw RSA signing. This algorithm does not do any hashing, it merely
     * encrypts its input, which should be a hash.
     */
    public static final SignatureAlgorithm
    RSASignature = new SignatureAlgorithm(SEC_OID_PKCS1_RSA_ENCRYPTION, "RSA",
            null, null, OBJECT_IDENTIFIER.PKCS1.subBranch(1)  );

    /**********************************************************************
     * Raw DSA signing. This algorithm does not do any hashing, it merely
     * encrypts its input, which should be a hash.
     */
    public static final SignatureAlgorithm
    DSASignature = new SignatureAlgorithm(SEC_OID_ANSIX9_DSA_SIGNATURE, "DSA",
        null, null, ANSI_X9_ALGORITHM.subBranch(1) );

    //////////////////////////////////////////////////////////////////////
    public static final SignatureAlgorithm
    RSASignatureWithMD2Digest =
        new SignatureAlgorithm(SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION,
                "RSASignatureWithMD2Digest", RSASignature, DigestAlgorithm.MD2,
                OBJECT_IDENTIFIER.PKCS1.subBranch(2) );

    //////////////////////////////////////////////////////////////////////
    public static final SignatureAlgorithm
    RSASignatureWithMD5Digest =
        new SignatureAlgorithm(SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION,
                "RSASignatureWithMD5Digest", RSASignature, DigestAlgorithm.MD5,
                OBJECT_IDENTIFIER.PKCS1.subBranch(4) );

    //////////////////////////////////////////////////////////////////////
    public static final SignatureAlgorithm
    RSASignatureWithSHA1Digest =
        new SignatureAlgorithm(SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION,
            "RSASignatureWithSHA1Digest", RSASignature, DigestAlgorithm.SHA1,
            OBJECT_IDENTIFIER.PKCS1.subBranch(5) );

    //////////////////////////////////////////////////////////////////////
    public static final SignatureAlgorithm
    DSASignatureWithSHA1Digest =
        new SignatureAlgorithm(SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST,
            "DSASignatureWithSHA1Digest", DSASignature, DigestAlgorithm.SHA1,
            ANSI_X9_ALGORITHM.subBranch(3) );

    //////////////////////////////////////////////////////////////////////
    public static final SignatureAlgorithm
    RSASignatureWithSHA256Digest =
        new SignatureAlgorithm(SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION,
            "RSASignatureWithSHA256Digest", RSASignature, DigestAlgorithm.SHA256,
            OBJECT_IDENTIFIER.PKCS1.subBranch(11));

    //////////////////////////////////////////////////////////////////////
    public static final SignatureAlgorithm
    RSASignatureWithSHA384Digest =
        new SignatureAlgorithm(SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION,
            "RSASignatureWithSHA384Digest", RSASignature, DigestAlgorithm.SHA384,
            OBJECT_IDENTIFIER.PKCS1.subBranch(12));
    
    //////////////////////////////////////////////////////////////////////
    public static final SignatureAlgorithm
    RSASignatureWithSHA512Digest =
        new SignatureAlgorithm(SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION,
            "RSASignatureWithSHA512Digest", RSASignature, DigestAlgorithm.SHA512,
            OBJECT_IDENTIFIER.PKCS1.subBranch(13));

}
