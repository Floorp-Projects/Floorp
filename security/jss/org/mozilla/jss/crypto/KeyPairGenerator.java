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

import java.security.*;
import java.security.SecureRandom;
import java.security.spec.AlgorithmParameterSpec;

/**
 * Generates RSA and DSA key pairs.  Each CryptoToken provides a
 * KeyPairGenerator, which can be used to generate key pairs on that token.
 * A given token may not support all algorithms, and some tokens may not
 * support any key pair generation. If a token does not support key pair
 * generation, the Netscape internal token may do it instead. Call
 * <code>keygenOnInternalToken</code> to find out if this is happening.
 *
 * @see org.mozilla.jss.crypto.CryptoToken#getKeyPairGenerator
 */
public class KeyPairGenerator {

    /**
     * Creates a new key pair generator.  KeyPairGenerators should
     * be obtained by calling <code>CryptoToken.getKeyPairGenerator</code>
     * instead of calling this constructor.
     *
     * @param algorithm The type of keys that the generator will be
     *      used to generate.
     * @param engine The engine object that provides the implementation for
     *      the class.
     */
	public KeyPairGenerator(KeyPairAlgorithm algorithm,
							KeyPairGeneratorSpi engine) {
		this.algorithm = algorithm;
		this.engine = engine;
	}

    /**
     * Generates a new key pair.
     *
     * @return A new key pair. The keys reside on the CryptoToken that 
     *      provided this <code>KeyPairGenerator</code>.
     * @exception TokenException If an error occurs on the CryptoToken
     *      in the process of generating the key pair.
     */
	public java.security.KeyPair
	genKeyPair() throws TokenException {
		return engine.generateKeyPair();
	}

    /**
     * @return The type of key that this generator generates.
     */
	public KeyPairAlgorithm getAlgorithm() {
		return algorithm;
	}

    /**
     * Initializes the generator with algorithm-specific parameters.
     *  The <tt>SecureRandom</tt> parameters is ignored.
     *
     * @param params Algorithm-specific parameters for the key pair generation.
     * @param random <b>This parameter is ignored.</b> NSS does not accept
     *      an external source of random numbers.
     * @exception InvalidAlgorithmParameterException If the parameters are
     *      inappropriate for the type of key pair that is being generated,
     *      or they are not supported by this generator.
     * @see org.mozilla.jss.crypto.RSAParameterSpec
     * @see java.security.spec.DSAParameterSpec
     */
	public void initialize(AlgorithmParameterSpec params, SecureRandom random)
            throws InvalidAlgorithmParameterException
    {
		engine.initialize(params, random);
	}

    /**
     * Initializes the generator with algorithm-specific parameters.
     *
     * @param params Algorithm-specific parameters for the key pair generation.
     * @exception InvalidAlgorithmParameterException If the parameters are
     *      inappropriate for the type of key pair that is being generated,
     *      or they are not supported by this generator.
     * @see org.mozilla.jss.crypto.RSAParameterSpec
     * @see java.security.spec.DSAParameterSpec
     */
	public void initialize(AlgorithmParameterSpec params)
		throws InvalidAlgorithmParameterException
	{
		engine.initialize(params, null);
	}

    /**
     * Initializes the generator with the strength of the keys.
     *      The <tt>SecureRandom</tt> parameter is ignored.
     *
     * @param strength The strength of the keys that will be generated.
     *      Usually this is the length of the key in bits.
     * @param random <b>This parameter is ignored.</b> NSS does not accept
     *      an exterrnal source of random numbers.
     */
	public void initialize(int strength, SecureRandom random) {
		engine.initialize(strength, random);
	}

    /**
     * Initializes the generator with the strength of the keys.
     *
     * @param strength The strength of the keys that will be generated.
     *      Usually this is the length of the key in bits.
     */
	public void initialize(int strength) {
		engine.initialize(strength, null);
	}

    /**
     * @return true if the keypair generation will take place on the 
     *      internal token rather than the current token.  This will
     *      happen if the token does not support keypair generation
     *      but does support this algorithm and is writable.  In this
     *      case the keypair will be generated on the Netscape internal
     *      token and then moved to this token.
     */
    public boolean keygenOnInternalToken() {
        return engine.keygenOnInternalToken();
    }

    /**
     * Tells the generator to generate temporary, rather than permanent,
     * keypairs.  Temporary keys are not written permanently to the token.
     * They are destroyed by the garbage collector.
     */
    public void temporaryPairs(boolean temp) {
        engine.temporaryPairs(temp);
    }

	protected KeyPairAlgorithm algorithm;
	protected KeyPairGeneratorSpi engine;
}
