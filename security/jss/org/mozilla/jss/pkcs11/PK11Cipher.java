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
import org.mozilla.jss.util.NativeProxy;
import java.security.InvalidKeyException;
import java.security.InvalidAlgorithmParameterException;
import java.security.spec.AlgorithmParameterSpec;
import org.mozilla.jss.util.Assert;
import java.security.NoSuchAlgorithmException;
import javax.crypto.spec.*;

final class PK11Cipher extends org.mozilla.jss.crypto.Cipher {

    // set once in the constructor
    private PK11Token token;

    // set once in the constructor
    private EncryptionAlgorithm algorithm;

    // set with initXXX()
    private AlgorithmParameterSpec parameters=null;

    // set with initXXX()
    private SymmetricKey key=null;

    // set with initXXX()
    private byte[] IV=null;

    // set with initXXX()
    private CipherContextProxy contextProxy = null;

    // modified by various operations
    private int state=UNINITIALIZED;

    // States
    private static final int UNINITIALIZED=0;
    private static final int ENCRYPT=1;
    private static final int DECRYPT=2;

    private PK11Cipher() { }

    PK11Cipher(PK11Token token, EncryptionAlgorithm algorithm) {
        this.token = token;
        this.algorithm = algorithm;
    }

    public void initEncrypt(SymmetricKey key)
        throws InvalidKeyException, InvalidAlgorithmParameterException,
        TokenException
    {
        initEncrypt(key, null);
    }

    public void initDecrypt(SymmetricKey key)
        throws InvalidKeyException, InvalidAlgorithmParameterException,
        TokenException
    {
        initDecrypt(key, null);
    }

    private static byte[] getIVFromParams(AlgorithmParameterSpec params) {
        byte[] IV = null;
        if( params instanceof IVParameterSpec ) {
            IV = ((IVParameterSpec)params).getIV();
        } else if( params instanceof IvParameterSpec ) {
            IV = ((IvParameterSpec)params).getIV();
        } else if( params instanceof RC2ParameterSpec ) {
            IV = ((RC2ParameterSpec)params).getIV();
        }
        return IV;
    }
        

    public void initEncrypt(SymmetricKey key, AlgorithmParameterSpec parameters)
        throws InvalidKeyException, InvalidAlgorithmParameterException,
        TokenException
    {
        reset();

        checkKey(key);
        checkParams(parameters);

        IV = getIVFromParams(parameters);
        this.key = key;
        this.parameters = parameters;
        state = ENCRYPT;

        if( parameters instanceof RC2ParameterSpec ) {
            contextProxy = initContextWithKeyBits( true, key, algorithm, IV,
                ((RC2ParameterSpec)parameters).getEffectiveKeyBits() );
        } else {
            contextProxy = initContext( true, key, algorithm, IV );
        }
    }

    public void initDecrypt(SymmetricKey key, AlgorithmParameterSpec parameters)
        throws InvalidKeyException, InvalidAlgorithmParameterException,
        TokenException
    {
        reset();

        checkKey(key);
        checkParams(parameters);

        IV = getIVFromParams(parameters);
        this.key = key;
        this.parameters = parameters;
        state = DECRYPT;

        if( parameters instanceof RC2ParameterSpec ) {
            contextProxy = initContextWithKeyBits(false, key, algorithm, IV,
                ((RC2ParameterSpec)parameters).getEffectiveKeyBits() );
        } else {
            contextProxy = initContext(false, key, algorithm, IV);
        }
    }

    public byte[] update(byte[] bytes)
        throws IllegalStateException, TokenException
    {
        if( state == UNINITIALIZED ) {
            throw new IllegalStateException();
        }

        return updateContext( contextProxy, bytes, algorithm.getBlockSize());
    }

    public byte[] update(byte[] bytes, int offset, int length)
        throws IllegalStateException, TokenException
    {
        byte[] sub = new byte[length];

        System.arraycopy( bytes, offset, sub, 0, length );

        return update(sub);
    }

    public byte[] doFinal(byte[] bytes)
        throws IllegalStateException, IllegalBlockSizeException,
        BadPaddingException, TokenException
    {
        if( state == UNINITIALIZED ) {
            throw new IllegalStateException();
        }

        byte[] first = update(bytes);

        byte[] last = finalizeContext(contextProxy, algorithm.getBlockSize(),
                    algorithm.isPadded() );

        byte[] combined = new byte[ first.length+last.length ];
        System.arraycopy(first, 0, combined, 0, first.length);
        System.arraycopy(last, 0, combined, first.length, last.length);
        return combined;
    }

    public byte[] doFinal(byte[] bytes, int offset, int length)
        throws IllegalStateException, IllegalBlockSizeException,
        BadPaddingException, TokenException
    {
        byte[] sub = new byte[length];

        System.arraycopy(bytes, offset, sub, 0, length);

        return doFinal(sub);
    }

    public byte[] doFinal()
        throws IllegalStateException, IllegalBlockSizeException,
        BadPaddingException, TokenException
    {
        if( state == UNINITIALIZED ) {
            throw new IllegalStateException();
        }
        return finalizeContext(contextProxy, algorithm.getBlockSize(),
                    algorithm.isPadded() );
    }

    private static native CipherContextProxy
    initContext(boolean encrypt, SymmetricKey key, EncryptionAlgorithm alg,
                 byte[] IV)
        throws TokenException;

    // This version accepts the number of effective key bits for RC2 CBC.
    private static native CipherContextProxy
    initContextWithKeyBits(boolean encrypt, SymmetricKey key,
                EncryptionAlgorithm alg, byte[] IV, int keyBits)
        throws TokenException;

    private static native byte[]
    updateContext( CipherContextProxy context, byte[] input, int blocksize )
        throws TokenException;

    private static native byte[]
    finalizeContext( CipherContextProxy context, int blocksize, boolean padded)
        throws TokenException, IllegalBlockSizeException, BadPaddingException;

    private void reset() {
        parameters = null;
        key = null;
        IV = null;
        state = UNINITIALIZED;
        contextProxy = null;
    }

    /**
     * Matches the params against those expected by the algorithm. 
     */
    private void checkParams(AlgorithmParameterSpec params)
        throws InvalidAlgorithmParameterException
    {
        if( ! algorithm.isValidParameterObject(params) ) {
            String name = "null";
            if( params != null ) {
                name = params.getClass().getName();
            }
            throw new InvalidAlgorithmParameterException(algorithm +
                " cannot use a " + name + " parameter");
        }
    }
    
            

    /**
     * Checks for null, makes sure the key lives on the correct token,
     * makes sure it is a PKCS #11 key, makes sure it's the right type
     * for this algorithm.
     */
    private void checkKey(SymmetricKey key) throws InvalidKeyException {
        if( key==null ) {
            throw new InvalidKeyException("Key is null");
        }
        if( ! key.getOwningToken().equals(token) ) {
            throw new InvalidKeyException("Key does not reside on the "+
                "current token");
        }
        if( ! (key instanceof PK11SymKey) ) {
            throw new InvalidKeyException("Key is not a PKCS #11 key");
        }
        try {
            if( ((PK11SymKey)key).getKeyType() !=
                    KeyType.getKeyTypeFromAlgorithm(algorithm) ) {
                throw new InvalidKeyException("Key is not the right type for"+
                    " this algorithm");
            }
        } catch( NoSuchAlgorithmException e ) {
            Assert.notReached("Unknown algorithm");
        }
    }

}
