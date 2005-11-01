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

package org.mozilla.jss.pkcs11;

import org.mozilla.jss.crypto.*;
import java.security.InvalidKeyException;
import java.security.InvalidAlgorithmParameterException;
import java.security.spec.AlgorithmParameterSpec;
import org.mozilla.jss.util.Password;
import org.mozilla.jss.util.UTF8Converter;
import java.io.CharConversionException;

public final class PK11KeyGenerator implements KeyGenerator {

    // opFlag constants: each of these flags specifies a crypto operation
    // the key will support.  Their values must match the same-named C
    // preprocessor macros defined in the PKCS #11 header pkcs11t.h.
    private static final int CKF_ENCRYPT = 0x00000100;
    private static final int CKF_DECRYPT = 0x00000200;
    private static final int CKF_SIGN = 0x00000800;
    private static final int CKF_VERIFY = 0x00002000;
    private static final int CKF_WRAP = 0x00020000;
    private static final int CKF_UNWRAP = 0x00040000;

    // A table for mapping SymmetricKey.Usage to opFlag.  This must be
    // synchronized with SymmetricKey.Usage.
    private static final int opFlagForUsage[] = {
        CKF_ENCRYPT,    /* 0 */
        CKF_DECRYPT,    /* 1 */
        CKF_WRAP,       /* 2 */
        CKF_UNWRAP,     /* 3 */
        CKF_SIGN,       /* 4 */
        CKF_VERIFY      /* 5 */
    };

    // The token this key will be generated on.
    private PK11Token token;

    // The algorithm to use to generate the key
    private KeyGenAlgorithm algorithm;

    // The strength of the key to be generated in bits.  A value of 0 means
    // that the strength has not been set.  This is OK for most algorithms.
    private int strength=0;

    // The parameters for this algorithm. May be null for some algorithms.
    private AlgorithmParameterSpec parameters;

    // The crypto operations the key will support.  It is the logical OR
    // of the opFlag constants, each specifying a supported operation.
    private int opFlags = CKF_SIGN | CKF_ENCRYPT;

    // Whether the key will be temporary or permanent
    private boolean temporaryKeyMode = true;

    // Used to convert Java Password into a byte[].
    private KeyGenerator.CharToByteConverter charToByte;

    private PK11KeyGenerator() { }

    // package private constructor
    PK11KeyGenerator(PK11Token token, KeyGenAlgorithm algorithm) {
        if( token==null || algorithm==null ) {
            throw new NullPointerException();
        }
        this.token = token;
        this.algorithm = algorithm;
        charToByte = new KeyGenerator.CharToByteConverter() {
            public byte[] convert(char[] chars) throws CharConversionException {
                return UTF8Converter.UnicodeToUTF8(chars);
            }
        };
    }


    /**
     * Sets the character to byte converter for passwords. The default
     * conversion is UTF8 with no null termination.
     */
    public void setCharToByteConverter(
                    KeyGenerator.CharToByteConverter charToByte)
    {
        if( charToByte==null ) {
            throw new IllegalArgumentException("CharToByteConverter is null");
        }
        this.charToByte = charToByte;
    }

    /**
     * @param strength Key size in bits. Must be evenly divisible by 8.
     */
    public void initialize(int strength)
        throws InvalidAlgorithmParameterException
    {
        // if this algorithm only accepts PBE key gen params, it can't
        // use a strength
        Class[] paramClasses = algorithm.getParameterClasses();
        if( paramClasses.length == 1 &&
                paramClasses[0].equals(PBEKeyGenParams.class) )
        {
            throw new InvalidAlgorithmParameterException("PBE keygen "+
                "algorithms require PBEKeyGenParams ");
        }

        // validate the strength for our algorithm
        if( ! algorithm.isValidStrength(strength) ) {
            throw new InvalidAlgorithmParameterException(strength+
                " is not a valid strength for "+algorithm);
        }

        if( strength % 8 != 0 ) {
            throw new InvalidAlgorithmParameterException(
                "Key strength must be divisible by 8");
        }

        this.strength = strength;
    }

    public void initialize(AlgorithmParameterSpec parameters)
        throws InvalidAlgorithmParameterException
    {
        if( ! algorithm.isValidParameterObject(parameters) ) {
            String name = "null";
            if( parameters != null ) {
                name = parameters.getClass().getName();
            }
            throw new InvalidAlgorithmParameterException(
                algorithm + " cannot use a " + name + " parameter");
        }
        this.parameters = parameters;
    }

    public void setKeyUsages(SymmetricKey.Usage[] usages)
    {
        this.opFlags = 0;
        for( int i = 0; i < usages.length; i++ ) {
            if( usages[i] != null ) {
                this.opFlags |= opFlagForUsage[usages[i].getVal()];
            }
        }
    }

    public void temporaryKeys(boolean temp)
    {
        this.temporaryKeyMode = temp;
    }


    /**
     * Generates the key. This is the public interface, the actual
     * work is done by native methods.
     */
    public SymmetricKey generate()
        throws IllegalStateException, TokenException, CharConversionException
    {
        Class[] paramClasses = algorithm.getParameterClasses();
        if( paramClasses.length == 1 &&
            paramClasses[0].equals(PBEKeyGenParams.class) )
        {
            if(parameters==null || !(parameters instanceof PBEKeyGenParams)) {
                throw new IllegalStateException(
                    "PBE keygen algorithms require PBEKeyGenParams");
            }
            PBEKeyGenParams kgp = (PBEKeyGenParams)parameters;

            byte[] pwbytes=null;
            try {
                pwbytes = charToByte.convert( kgp.getPassword().getChars() );
                return generatePBE(token, algorithm, pwbytes,
                    kgp.getSalt(), kgp.getIterations());
            } finally {
                if( pwbytes!=null ) {
                    Password.wipeBytes(pwbytes);
                }
            }
        } else {
            return generateNormal(token, algorithm, strength,
                opFlags, temporaryKeyMode);
        }
    }

    /**
     * Generates an Initialization Vector using a PBE algorithm.
     * In order to call this method, the algorithm must be a PBE algorithm,
     * and the KeyGenerator must have been initialized with an instance
     * of <code>PBEKeyGenParams</code>.
     * 
     * @return The initialization vector derived from the password and salt
     *      using the PBE algorithm.
     */
    public byte[] generatePBE_IV()
        throws TokenException, CharConversionException
    {
        Class[] paramClasses = algorithm.getParameterClasses();
        if( paramClasses.length == 1 &&
            paramClasses[0].equals(PBEKeyGenParams.class) )
        {
            if(parameters==null || !(parameters instanceof PBEKeyGenParams)) {
                throw new IllegalStateException(
                    "PBE keygen algorithms require PBEKeyGenParams");
            }
            PBEKeyGenParams kgp = (PBEKeyGenParams)parameters;

            byte[] pwbytes=null;
            try {
                pwbytes = charToByte.convert(kgp.getPassword().getChars());
                return generatePBE_IV(algorithm, pwbytes, kgp.getSalt(),
                                    kgp.getIterations() );
            } finally {
                if(pwbytes!=null) {
                    Password.wipeBytes(pwbytes);
                }
            }
        } else {
            throw new IllegalStateException(
                "IV generation can only be performed by PBE algorithms");
        }
    }

    /**
     * A native method to generate an IV using a PBE algorithm.
     * None of the parameters should be NULL.
     */
    private static native byte[]
    generatePBE_IV(KeyGenAlgorithm alg, byte[] password, byte[] salt,
                    int iterations) throws TokenException;

    /**
     * Allows a SymmetricKey to be cloned on a different token.
     *
     * @exception SymmetricKey.NotExtractableException If the key material
     *      cannot be extracted from the current token.
     * @exception InvalidKeyException If the owning token cannot process
     *      the key to be cloned.
     */
    public SymmetricKey clone(SymmetricKey key)
        throws SymmetricKey.NotExtractableException,
            InvalidKeyException, TokenException
    {
        return clone(key, token);
    }

    /**
     * Allows a SymmetricKey to be cloned on a different token.
     *
     * @param key The key to clone.
     * @param token The token on which to clone the key.
     * @exception SymmetricKey.NotExtractableException If the key material
     *      cannot be extracted from the current token.
     * @exception InvalidKeyException If the owning token cannot process
     *      the key to be cloned.
     */
    public static SymmetricKey clone(SymmetricKey key, PK11Token token)
        throws SymmetricKey.NotExtractableException, InvalidKeyException,
            TokenException
    {
        if( ! (key instanceof PK11SymKey) ) {
            throw new InvalidKeyException("Key is not a PKCS #11 key");
        }
        return nativeClone(token, key);
    }
    
    private static native SymmetricKey
    nativeClone(PK11Token token, SymmetricKey toBeCloned)
        throws SymmetricKey.NotExtractableException, TokenException;


    /**
     * A native method to generate a non-PBE key.
     * @param token The token where the key generation happens
     * @param algorithm The algorithm to use
     * @param strength The key size in bits, should be 0 for fixed-length
     *      key algorithms.
     * @param opFlags The crypto operations the key will support
     * @param temporary Whether the key will be temporary or permanent
     */
    private static native SymmetricKey
    generateNormal(PK11Token token, KeyGenAlgorithm algorithm, int strength,
        int opFlags, boolean temporary)
        throws TokenException;

    /**
     * A native method to generate a PBE key. None of the parameters should
     *  be null.
     */
    private static native SymmetricKey
    generatePBE(PK11Token token, KeyGenAlgorithm algorithm, byte[] pass,
        byte[] salt, int iterationCount) throws TokenException;

}
