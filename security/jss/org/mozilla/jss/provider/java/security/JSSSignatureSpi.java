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
 * Portions created by the Initial Developer are Copyright (C) 1998-2002
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

import org.mozilla.jss.crypto.PrivateKey;
import java.security.*;
import java.security.spec.*;
import org.mozilla.jss.crypto.*;

class JSSSignatureSpi extends java.security.SignatureSpi {

    org.mozilla.jss.crypto.Signature sig;
    SignatureAlgorithm alg;

    private JSSSignatureSpi() { }

    protected JSSSignatureSpi(SignatureAlgorithm alg) {
        this.alg = alg;
    }

    public Object clone() throws CloneNotSupportedException {
        throw new CloneNotSupportedException();
    }

    public void engineInitSign(java.security.PrivateKey privateKey,
        SecureRandom random) throws InvalidKeyException
    {
        // discard the random
        engineInitSign(privateKey);
    }

    public void engineInitSign(java.security.PrivateKey privateKey)
        throws InvalidKeyException
    {
        try {
            sig = getSigContext(privateKey);
            sig.initSign((PrivateKey)privateKey);
        } catch(java.security.NoSuchAlgorithmException e) {
            throw new InvalidKeyException("Algorithm not supported");
        } catch(TokenException e) {
            throw new InvalidKeyException("Token exception occurred");
        }
    }

    private org.mozilla.jss.crypto.Signature
    getSigContext(java.security.PrivateKey privateKey)
        throws NoSuchAlgorithmException, InvalidKeyException, TokenException
    {
        CryptoToken token;
        PrivateKey privk;

        if( ! (privateKey instanceof PrivateKey) ) {
            throw new InvalidKeyException();
        }
        privk = (PrivateKey)privateKey;

        token = privk.getOwningToken();

        return token.getSignatureContext(alg);
    }

    public void engineInitVerify(PublicKey publicKey)
        throws InvalidKeyException
    {
        try {
            CryptoToken token =
              TokenSupplierManager.getTokenSupplier().getThreadToken();
            sig = token.getSignatureContext(alg);

            // convert the public key into a JSS public key if necessary
            if( ! (publicKey instanceof org.mozilla.jss.pkcs11.PK11PubKey) ) {
                if( ! publicKey.getFormat().equalsIgnoreCase("X.509") ) {
                    throw new InvalidKeyException(
                        "Unsupported public key format: " +
                        publicKey.getFormat());
                }
                X509EncodedKeySpec encodedKey =
                    new X509EncodedKeySpec(publicKey.getEncoded());
                KeyFactory fact = KeyFactory.getInstance(
                    publicKey.getAlgorithm(), "Mozilla-JSS");
                publicKey = fact.generatePublic(encodedKey);
            }

            sig.initVerify(publicKey);
        } catch(NoSuchProviderException e) {
            throw new InvalidKeyException("Unable to convert non-JSS key " +
                "to JSS key");
        } catch(java.security.spec.InvalidKeySpecException e) {
            throw new InvalidKeyException("Unable to convert non-JSS key " +
                "to JSS key");
        } catch(java.security.NoSuchAlgorithmException e) {
            throw new InvalidKeyException("Algorithm not supported");
        } catch(TokenException e) {
            throw new InvalidKeyException("Token exception occurred");
        }
    }

    public void engineUpdate(byte b)
        throws SignatureException
    {
        try {
            sig.update(b);
        } catch( TokenException e) {
            throw new SignatureException("TokenException: "+e.toString());
        }
    }

    public void engineUpdate(byte[] b, int off, int len)
        throws SignatureException
    {
        try {
            sig.update(b, off, len);
        } catch( TokenException e) {
            throw new SignatureException("TokenException: "+e.toString());
        }
    }

    public byte[] engineSign() throws SignatureException {
        try {
            return sig.sign();
        } catch(TokenException e) {
            throw new SignatureException("TokenException: "+e.toString());
        }
    }

    public int engineSign(byte[] outbuf, int offset, int len)
        throws SignatureException
    {
        try {
            return sig.sign(outbuf, offset, len);
        } catch(TokenException e) {
            throw new SignatureException("TokenException: "+e.toString());
        }
    }

    public boolean engineVerify(byte[] sigBytes) throws SignatureException {
        try {
            return sig.verify(sigBytes);
        } catch( TokenException  e) {
            throw new SignatureException("TokenException: "+e.toString());
        }
    }

    public void engineSetParameter(AlgorithmParameterSpec params)
        throws InvalidAlgorithmParameterException
    {
        try {
            sig.setParameter(params);
        } catch( TokenException e ) {
            throw new InvalidAlgorithmParameterException(
                "TokenException: "+e.toString());
        }
    }

    public Object engineGetParameter(String param)
            throws InvalidParameterException
    {
        throw new InvalidParameterException(
            "name/value parameters not supported");
    }

    public void engineSetParameter(String param, Object value)
            throws InvalidParameterException
    {
        throw new InvalidParameterException(
            "name/value parameters not supported");
    }

    public static class DSA extends JSSSignatureSpi {
        public DSA() {
            super(SignatureAlgorithm.DSASignatureWithSHA1Digest);
        }
    }
    public static class MD2RSA extends JSSSignatureSpi {
        public MD2RSA() {
            super(SignatureAlgorithm.RSASignatureWithMD2Digest);
        }
    }
    public static class MD5RSA extends JSSSignatureSpi {
        public MD5RSA() {
            super(SignatureAlgorithm.RSASignatureWithMD5Digest);
        }
    }
    public static class SHA1RSA extends JSSSignatureSpi {
        public SHA1RSA() {
            super(SignatureAlgorithm.RSASignatureWithSHA1Digest);
        }
    }
    public static class SHA256RSA extends JSSSignatureSpi {
        public SHA256RSA() {
            super(SignatureAlgorithm.RSASignatureWithSHA256Digest);
        }
    }
    public static class SHA384RSA extends JSSSignatureSpi {
        public SHA384RSA() {
            super(SignatureAlgorithm.RSASignatureWithSHA384Digest);
        }
    }
    public static class SHA512RSA extends JSSSignatureSpi {
        public SHA512RSA() {
            super(SignatureAlgorithm.RSASignatureWithSHA512Digest);
        }
    }


}
