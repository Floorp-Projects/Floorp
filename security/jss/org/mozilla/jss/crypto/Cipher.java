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

import java.security.InvalidKeyException;
import java.security.spec.AlgorithmParameterSpec;
import java.security.InvalidAlgorithmParameterException;
import org.mozilla.jss.util.Assert;

/**
 * A context for performing symmetric encryption and decryption.
 * First, the context must be initialized. Then, it can be updated
 * with input through zero or more calls to <code>update</code>.  Finally,
 * <code>doFinal</code> is called to finalize the operation. Note that
 * it is not necessary to call <code>update</code> if all of the data is
 * available at once.  In this case, all of the input can be processed with one
 * call to <code>doFinal</code>.
 */
public abstract class Cipher {

    /**
     * Initializes a encryption context with a symmetric key.
     */
    public abstract void initEncrypt(SymmetricKey key)
        throws InvalidKeyException, InvalidAlgorithmParameterException,
        TokenException;

    /**
     * Initializes a decryption context with a symmetric key.
     */
    public abstract void initDecrypt(SymmetricKey key)
        throws InvalidKeyException, InvalidAlgorithmParameterException,
        TokenException;

    /**
     * Initializes an encryption context with a symmetric key and
     * algorithm parameters.
     */
    public abstract void
    initEncrypt(SymmetricKey key, AlgorithmParameterSpec parameters)
        throws InvalidKeyException, InvalidAlgorithmParameterException,
        TokenException;

    /** 
     * Initializes a decryption context with a symmetric key and
     * algorithm parameters.
     */
    public abstract void
    initDecrypt(SymmetricKey key, AlgorithmParameterSpec parameters)
        throws InvalidKeyException, InvalidAlgorithmParameterException,
        TokenException;

    /**
     * Updates the encryption context with additional input.
     * @param bytes Bytes of plaintext (if encrypting) or ciphertext (if
     *      decrypting).
     * @return Bytes of ciphertext (if encrypting) or plaintext (if decrypting).
     */
    public abstract byte[] update(byte[] bytes)
        throws IllegalStateException, TokenException;

    /**
     * Updates the encryption context with additional plaintext.
     * @param bytes Bytes of plaintext (if encrypting) or ciphertext (if
     *      decrypting).
     * @param offset The index in <code>bytes</code> at which to begin reading.
     * @param length The number of bytes from <code>bytes</code> to read.
     * @return Bytes of ciphertext (if encrypting) or plaintext (if decrypting).
     */
    public abstract byte[] update(byte[] bytes, int offset, int length)
        throws IllegalStateException, TokenException;

    /**
     * Completes an cipher operation. This can be called directly after
     *  the context is initialized, or <code>update</code> may be called
     *  any number of times before calling <code>final</code>.
     * @param bytes Bytes of plaintext (if encrypting) or ciphertext (if
     *      decrypting).
     * @return The last of the output.
     */
    public abstract byte[] doFinal(byte[] bytes)
        throws IllegalStateException, IllegalBlockSizeException,
        BadPaddingException, TokenException;

    /**
     * Completes an cipher operation.
     * @param bytes Bytes of plaintext (if encrypting) or ciphertext (if
     *      decrypting).
     * @param offset The index in <code>bytes</code> at which to begin reading.
     * @param length The number of bytes from <code>bytes</code> to read.
     * @return The last of the output.
     */
    public abstract byte[] doFinal(byte[] bytes, int offset, int length)
        throws IllegalStateException, IllegalBlockSizeException,
        BadPaddingException, TokenException;

    /**
     * Completes an cipher operation.
     * @return The last of the output.
     */
    public abstract byte[] doFinal()
        throws IllegalStateException, IllegalBlockSizeException,
        BadPaddingException, TokenException;

    /**
     * Pads a byte array so that its length is a multiple of the given
     *  blocksize.  The method of padding is the one defined in the RSA
     *  PKCS standards. If <i>M</i> is the length of the data and
     *  <i>B</i> is the block size, the padding string consists of
     *  <i>B</i> - (<i>M</i> mod <i>B</i>) octets, each having the value
     *  <i>B</i> - (<i>M</i> mod <i>B</i>).
     * @param The block size of the encryption algorithm.  Must be greater
     *  than zero.
     * @see #unPad
     */
    public static byte[]
    pad(byte[] toBePadded, int blockSize) {
        Assert._assert(blockSize > 0);

        // the padOctet is also the number of pad octets
        byte padOctet = (byte) (blockSize - (toBePadded.length % blockSize));

        byte[] padded = new byte[toBePadded.length + padOctet];

        System.arraycopy(toBePadded, 0, padded, 0, toBePadded.length);

        for(int i = toBePadded.length; i < padded.length; i++) {
            padded[i] = padOctet;
        }

        return padded;
    }

    /**
     * Un-pads a byte array that is padded with PKCS padding.
     *
     * @param blockSize The block size of the encryption algorithm. This
     *      is only used for error checking: if the pad size is not
     *      between 1 and blockSize, a BadPaddingException is thrown.
     *
     * @see #pad
     */
    public static byte[]
    unPad(byte[] padded, int blockSize) throws BadPaddingException {
        if(padded.length == 0) {
            return new byte[0];
        }

        if( padded.length < blockSize ) {
            throw new BadPaddingException("Length of padded array is less than"+
                " one block");
        }
        byte padOctet = padded[padded.length-1];
        if(padOctet > blockSize) {
            throw new BadPaddingException("Padding octet ("+padOctet+") is "+
                "larger than block size ("+blockSize+")");
        }
        if(padOctet < 1) {
            throw new BadPaddingException("Padding octet is less than 1");
        }

        byte[] unpadded = new byte[padded.length - padOctet];

        System.arraycopy(padded, 0, unpadded, 0, unpadded.length);

        return unpadded;
    }

    /**
     * Un-pads a byte array that is padded with PKCS padding. Since
     * this version does not take block size as a parameter, it cannot
     * error check.
     * @see #pad
     */
    public static byte[]
    unPad(byte[] padded) throws BadPaddingException {
        if(padded.length == 0) {
            return new byte[0];
        }

        byte padOctet = padded[padded.length-1];
        if(padOctet < 1) {
            throw new BadPaddingException("Padding octet is less than 1");
        } else if(padOctet >= padded.length) {
            throw new BadPaddingException("Padding is larger than entire"+
                " array");
        }

        byte[] unpadded = new byte[padded.length - padOctet];

        System.arraycopy(padded, 0, unpadded, 0, unpadded.length);

        return unpadded;
    }
}
