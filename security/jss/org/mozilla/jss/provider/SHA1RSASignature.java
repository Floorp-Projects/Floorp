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
package org.mozilla.jss.provider;

import java.security.PrivateKey;
import java.security.SecureRandom;
import java.security.SignatureException;
import java.security.InvalidKeyException;
import java.security.PublicKey;
import java.security.InvalidParameterException;
import org.mozilla.jss.crypto.SignatureAlgorithm;
import java.security.spec.AlgorithmParameterSpec;
import java.security.InvalidAlgorithmParameterException;

public class SHA1RSASignature extends java.security.Signature {

    protected Signature sig = new Signature();

    public SHA1RSASignature() {
        super("SHA-1/RSA");
    }

    public void engineInitSign(PrivateKey privateKey, SecureRandom random)
        throws InvalidKeyException
    {
        sig.engineInitSign(privateKey, random,
            SignatureAlgorithm.RSASignatureWithSHA1Digest);
    }

    public void engineInitSign(PrivateKey privateKey)
        throws InvalidKeyException
    {
        sig.engineInitSign(privateKey,
            SignatureAlgorithm.RSASignatureWithSHA1Digest);
    }

    public void engineInitVerify(PublicKey publicKey)
        throws InvalidKeyException
    {
        sig.engineInitVerify(publicKey,
            SignatureAlgorithm.RSASignatureWithSHA1Digest);
    }

    public void engineUpdate(byte b)
        throws SignatureException
    {
        sig.engineUpdate(b);
    }

    public void engineUpdate(byte[] b, int off, int len)
        throws SignatureException
    {
        sig.engineUpdate(b, off, len);
    }

    public byte[] engineSign() throws SignatureException {
        return sig.engineSign();
    }

    public int engineSign(byte[] outbuf, int offset, int len)
        throws SignatureException
    {
        return sig.engineSign(outbuf, offset, len);
    }

    public boolean engineVerify(byte[] sigBytes) throws SignatureException {
        return sig.engineVerify(sigBytes);
    }

    public void engineSetParameter(AlgorithmParameterSpec params)
        throws InvalidAlgorithmParameterException
    {
        sig.engineSetParameter(params);
    }

    protected void engineSetParameter(String param, Object value)
        throws InvalidParameterException
    {
        throw new InvalidParameterException();
    }

    protected Object engineGetParameter(String param)
        throws InvalidParameterException
    {
        throw new InvalidParameterException();
    }
}
