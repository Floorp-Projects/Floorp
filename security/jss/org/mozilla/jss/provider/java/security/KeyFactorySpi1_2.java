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
 * The Original Code is Network Security Services for Java.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
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
package org.mozilla.jss.provider.java.security;

import java.security.PublicKey;
import java.security.spec.*;
import org.mozilla.jss.crypto.InvalidKeyFormatException;
import org.mozilla.jss.crypto.PrivateKey;
import org.mozilla.jss.crypto.TokenSupplierManager;
import org.mozilla.jss.crypto.SignatureAlgorithm;
import org.mozilla.jss.crypto.TokenException;
import org.mozilla.jss.asn1.*;
import org.mozilla.jss.pkcs11.PK11PubKey;
import org.mozilla.jss.pkcs11.PK11PrivKey;
import org.mozilla.jss.pkix.primitive.*;
import org.mozilla.jss.util.Assert;
import java.security.Key;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.math.BigInteger;
import java.io.StringWriter;
import java.io.PrintWriter;

public class KeyFactorySpi1_2 extends java.security.KeyFactorySpi
{

    protected PublicKey engineGeneratePublic(KeySpec keySpec)
        throws InvalidKeySpecException
    {
        if( keySpec instanceof RSAPublicKeySpec ) {
            RSAPublicKeySpec spec = (RSAPublicKeySpec) keySpec;

            // Generate a DER RSA public key
            SEQUENCE seq = new SEQUENCE();
            seq.addElement( new INTEGER(spec.getModulus()));
            seq.addElement( new INTEGER(spec.getPublicExponent()));

            return PK11PubKey.fromRaw( PrivateKey.RSA, ASN1Util.encode(seq) );
        } else if( keySpec instanceof DSAPublicKeySpec ) {
            // We need to import both the public value and the PQG parameters.
            // The only way to get all that information in DER is to send 
            // a full SubjectPublicKeyInfo. So we encode all the information
            // into an SPKI.

            DSAPublicKeySpec spec = (DSAPublicKeySpec) keySpec;

            SEQUENCE pqg = new SEQUENCE();
            pqg.addElement( new INTEGER(spec.getP()) );
            pqg.addElement( new INTEGER(spec.getQ()) );
            pqg.addElement( new INTEGER(spec.getG()) );
            OBJECT_IDENTIFIER oid = null;
            try {
                oid = SignatureAlgorithm.DSASignature.toOID();
            } catch(NoSuchAlgorithmException ex ) {
                Assert.notReached("no such algorithm as DSA?");
            }
            AlgorithmIdentifier algID = new AlgorithmIdentifier( oid, pqg );
            INTEGER publicValue = new INTEGER(spec.getY());
            byte[] encodedPublicValue = ASN1Util.encode(publicValue);
            SubjectPublicKeyInfo spki = new SubjectPublicKeyInfo(
                algID, new BIT_STRING(encodedPublicValue, 0) );

            return PK11PubKey.fromSPKI( ASN1Util.encode(spki) );
  	//
	// requires JAVA 1.5
	//
        //} else if( keySpec instanceof ECPublicKeySpec ) {
        //   // We need to import both the public value and the curve.
        //   // The only way to get all that information in DER is to send 
        //   // a full SubjectPublicKeyInfo. So we encode all the information
        //   // into an SPKI.
        //
        //  ECPublicKeySpec spec = (ECPublicKeySpec) keySpec;
	//    AlgorithmParameters algParams = getInstance("ECParameters");
        //
        //    algParameters.init(spec.getECParameters());
        //    OBJECT_IDENTIFIER oid = null;
        //    try {
        //        oid = SignatureAlgorithm.ECSignature.toOID();
        //    } catch(NoSuchAlgorithmException ex ) {
        //        Assert.notReached("no such algorithm as DSA?");
        //    }
        //    AlgorithmIdentifier algID = 
        //                  new AlgorithmIdentifier(oid, ecParams.getParams() );
        //    INTEGER publicValueX = new INTEGER(spec.getW().getAffineX());
        //    INTEGER publicValueY = new INTEGER(spec.getW().getAffineY());
        //    byte[] encodedPublicValue;
        //    encodedPublicValue[0] = EC_UNCOMPRESSED_POINT;
        //    encodedPublicValue += spec.getW().getAffineX().toByteArray();
        //    encodedPublicValue += spec.getW().getAffineY().toByteArray();
        //
        //    byte[] encodedPublicValue = ASN1Util.encode(publicValue);
        //    SubjectPublicKeyInfo spki = new SubjectPublicKeyInfo(
        //        algID, new BIT_STRING(encodedPublicValue, 0) );
        //
        //   return PK11PubKey.fromSPKI( ASN1Util.encode(spki) );
        //
        // use the following for EC keys in 1.4.2
        } else if( keySpec instanceof X509EncodedKeySpec ) {
            //
            // SubjectPublicKeyInfo
            //
            X509EncodedKeySpec spec = (X509EncodedKeySpec) keySpec;
            return PK11PubKey.fromSPKI( spec.getEncoded() );
        }
        throw new InvalidKeySpecException("Unsupported KeySpec type: " +
            keySpec.getClass().getName());
    }

    /**
     * We don't support RSAPrivateKeySpec because it doesn't have enough
     * information. You need to provide an RSAPrivateCrtKeySpec.
     */
    protected java.security.PrivateKey engineGeneratePrivate(KeySpec keySpec)
        throws InvalidKeySpecException
    {
      try {
        if( keySpec instanceof RSAPrivateCrtKeySpec ) {
            //
            // PKCS #1 RSAPrivateKey
            //
            RSAPrivateCrtKeySpec spec = (RSAPrivateCrtKeySpec) keySpec;
            SEQUENCE privKey = new SEQUENCE();
            privKey.addElement( new INTEGER(0) ) ; // version
            privKey.addElement( new INTEGER(spec.getModulus()) );
            privKey.addElement( new INTEGER(spec.getPublicExponent()) );
            privKey.addElement( new INTEGER(spec.getPrivateExponent()) );
            privKey.addElement( new INTEGER(spec.getPrimeP()) );
            privKey.addElement( new INTEGER(spec.getPrimeQ()) );
            privKey.addElement( new INTEGER(spec.getPrimeExponentP()) );
            privKey.addElement( new INTEGER(spec.getPrimeExponentQ()) );
            privKey.addElement( new INTEGER(spec.getCrtCoefficient()) );

            AlgorithmIdentifier algID =
                new AlgorithmIdentifier( PrivateKey.RSA.toOID(), null );

            OCTET_STRING encodedPrivKey = new OCTET_STRING(
                ASN1Util.encode(privKey) );
            PrivateKeyInfo pki = new PrivateKeyInfo(
                new INTEGER(0),     // version
                algID,
                encodedPrivKey,
                (SET)null                // OPTIONAL SET OF Attribute
            );
            return PK11PrivKey.fromPrivateKeyInfo( ASN1Util.encode(pki),
                TokenSupplierManager.getTokenSupplier().getThreadToken() );
        } else if( keySpec instanceof DSAPrivateKeySpec ) {
            DSAPrivateKeySpec spec = (DSAPrivateKeySpec) keySpec;
            SEQUENCE pqgParams = new SEQUENCE();
            pqgParams.addElement(new INTEGER(spec.getP()));
            pqgParams.addElement(new INTEGER(spec.getQ()));
            pqgParams.addElement(new INTEGER(spec.getG()));
            AlgorithmIdentifier algID =
                new AlgorithmIdentifier( PrivateKey.DSA.toOID(), pqgParams );
            OCTET_STRING privateKey = new OCTET_STRING(
                ASN1Util.encode(new INTEGER(spec.getX())) );

            PrivateKeyInfo pki = new PrivateKeyInfo(
                    new INTEGER(0),     // version
                    algID,
                    privateKey,
                    null                // OPTIONAL SET OF Attribute
            );

            // Derive the public key from the private key
            BigInteger y = spec.getG().modPow(spec.getX(), spec.getP());
            byte[] yBA = y.toByteArray();
            // we need to chop off a leading zero byte
            if( y.bitLength() % 8 == 0 ) {
                byte[] newBA = new byte[yBA.length-1];
                Assert._assert(newBA.length >= 0);
                System.arraycopy(yBA, 1, newBA, 0, newBA.length);
                yBA = newBA;
            }
            
            return PK11PrivKey.fromPrivateKeyInfo( ASN1Util.encode(pki),
                TokenSupplierManager.getTokenSupplier().getThreadToken(), yBA );
        } else if( keySpec instanceof PKCS8EncodedKeySpec ) {
            return PK11PrivKey.fromPrivateKeyInfo(
                (PKCS8EncodedKeySpec)keySpec,
                TokenSupplierManager.getTokenSupplier().getThreadToken() );
        }

        throw new InvalidKeySpecException("Unsupported KeySpec type: " +
            keySpec.getClass().getName());
      } catch(TokenException te) {
            StringWriter sw = new StringWriter();
            PrintWriter pw = new PrintWriter(sw);
            te.printStackTrace(pw);
            throw new InvalidKeySpecException("TokenException: " +
                sw.toString());
      }
    }

    protected KeySpec engineGetKeySpec(Key key, Class keySpec)
        throws InvalidKeySpecException
    {
        throw new InvalidKeySpecException(
            "Exporting raw key data is not supported. Wrap the key instead.");
    }

    /**
     * Translates key by calling getEncoded() to get its encoded form,
     * then importing the key from its encoding. Two formats are supported:
     * "X.509", which is decoded with an X509EncodedKeySpec;
     * and "PKCS#8", which is decoded with a PKCS8EncodedKeySpec.
     *
     * <p>This method is not well standardized: the documentation is very vague
     * about how the key is supposed to be translated. It is better
     * to move keys around by wrapping and unwrapping them; or by manually
     * translating to a KeySpec, then manually translating back to a Key.
     */
    protected Key engineTranslateKey(Key key)
        throws InvalidKeyException
    {
        byte[] encoded = key.getEncoded();
        String format = key.getFormat();

          try {
            if( format.equals("SubjectPublicKeyInfo") ||
                format.equalsIgnoreCase("X.509"))
            {
                X509EncodedKeySpec spec = new X509EncodedKeySpec(encoded);
                return engineGeneratePublic(spec);
            } else if( format.equals("PrivateKeyInfo") ||
                format.equalsIgnoreCase("PKCS#8"))
            {
                PKCS8EncodedKeySpec spec = new PKCS8EncodedKeySpec(encoded);
                return engineGeneratePrivate(spec);
            }
          } catch(InvalidKeySpecException e) {
            throw new InvalidKeyException(e.getMessage());
          }
        throw new InvalidKeyException(
            "Unsupported encoding format: " + format);
    }
}
