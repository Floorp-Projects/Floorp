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

import org.mozilla.jss.util.*;
import java.security.*;
import java.security.spec.AlgorithmParameterSpec;

/**
 * A class for producing and verifying digital signatures.
 * Instances of this class can be obtain from <code>CryptoToken</code>s.
 *
 * @see org.mozilla.jss.crypto.CryptoToken#getSignatureContext
 * @deprecated Use the JCA interface instead ({@link java.security.Signature})
 */
public class Signature { 

	protected Signature() { }

	Signature(SignatureAlgorithm algorithm, SignatureSpi engine) {
		this.algorithm = algorithm;
		this.engine = engine;
	}

	/**
	 * This is not supported yet.
	 */
	public Provider getProvider() {
		Assert.notYetImplemented("Signature.getProvider");
        return null;
	}

	/**
	 * Supplying sources of randoms is not supported yet.
	public void initSign(PrivateKey privateKey, SecureRandom random)
		throws InvalidKeyException, TokenException
	{
		engine.engineInitSign(privateKey, random);
	}
	*/

	/**
	 * Initialize the signature context for signing.
	 * @param privateKey The private key with which to sign.
	 * @exception InvalidKeyException If the key is the wrong type for the
	 * 		algorithm or does not exist on the token of this signature
	 *		context.
 	 * @exception TokenException If an error occurred on the token.
	 */
	public void initSign(PrivateKey privateKey)
        throws InvalidKeyException, TokenException
    {
		engine.engineInitSign(privateKey);
	}

	/**
	 * Initialize the signature context for verifying.
	 * @param publicKey The public key with which to verify the signature.
	 * @exception InvalidKeyException If the key is the wrong type for the
	 *		algorithm.
	 * @exception TokenException If an error occurs on the token.
	 */
	public void initVerify(PublicKey publicKey)
        throws InvalidKeyException, TokenException
    {
		engine.engineInitVerify(publicKey);
	}

	/**
	 * Set parameters for the signing algorithm. This is currently not
	 * supported or needed.
	 * @param params Parameters for the signing algorithm.
	 * @exception InvalidAlgorithmParameterException If there is something wrong
	 *		with the parameters.
	 * @exception TokenException If an error occurs on the token.
	 */
	public void setParameter(AlgorithmParameterSpec params)
        throws InvalidAlgorithmParameterException, TokenException
    {
		engine.engineSetParameter(params);
	}

	/**
	 * Finish a signing operation and return the signature.
	 * @exception SignatureException If an error occurs with the signing
	 *		operation.
	 * @exception TokenException If an error occurs on the token.
	 * @return The signature.
	 */
	public byte[] sign() throws SignatureException, TokenException
    {
		return engine.engineSign();
	}

	/**
	 * Finish a signing operation and store the signature in the provided
	 * buffer.
	 * @param outbuf Buffer to hold the signature
	 * @param offset Offset in buffer at which to store signature.
	 * @param len Number of bytes of buffer available for signature.
	 * @return int The number of bytes placed into outbuf.
	 * @exception SignatureException If an error occurred while signing, or
	 *		len was insufficient to contain the signature.
	 * @exception TokenException If an error occurred on the token.
	 */
	public int sign(byte[] outbuf, int offset, int len)
        throws SignatureException, TokenException
    {
		return engine.engineSign(outbuf, offset, len);
	}

	/**
	 * Finish a verification operation.
	 * @param signature The signature to be verified.
	 * @return true if the signature is valid, false if it is invalid.
	 * @exception SignatureException If an error occurred with the verification
	 * 		operation
	 * @exception TokenException If an error occurred on the token.
	 */
	public boolean verify(byte[] signature)
        throws SignatureException, TokenException
    {
		return engine.engineVerify(signature);
	}

	/**
	 * Provide more data for a signature or verification operation.
	 * @param b A byte to be signed or verified.
	 * @exception SignatureException If an error occurs in the
	 * 		signature/verifcation.
 	 * @exception TokenException If an error occurs on the token.
	 */
	public void update(byte b)
        throws SignatureException, TokenException
    {
		engine.engineUpdate(b);
	}

	/**
	 * Provide more data for a signature or verification operation.
	 * @param data An array of bytes to be signed or verified.
	 * @exception SignatureException If an error occurs in the
	 * 		signature/verifcation.
 	 * @exception TokenException If an error occurs on the token.
	 */
	public void update(byte[] data)
        throws SignatureException, TokenException
    {
		engine.engineUpdate(data, 0, data.length);
	}

	/**
	 * Provide more data for a signature or verification operation.
	 * @param data An array of bytes, some of which will be signed or verified.
	 * @param off The beginning offset of the bytes to be signed/verified.
	 * @param len The number of bytes to be signed/verified.
	 * @exception SignatureException If an error occurs in the
	 * 		signature/verifcation.
 	 * @exception TokenException If an error occurs on the token.
	 */
	public void update(byte[] data, int off, int len)
		throws SignatureException, TokenException
	{
		engine.engineUpdate(data, off, len);
	}

	/**
	 * Returns the name of the algorithm to be used for signing.
	 */
	public String getAlgorithm() {
		return algorithm.toString();
	}

	/**
	 * Returns the algorithm to be used for signing.
	 */
	public SignatureAlgorithm getAlgorithmID() {
		return algorithm;
	}

	/**
	 * Cloning is not supported yet
	 */
	protected Object clone() throws CloneNotSupportedException {
		// no cloning for now
		throw new CloneNotSupportedException();
	}

	protected SignatureAlgorithm algorithm;
	protected SignatureSpi engine;
}
