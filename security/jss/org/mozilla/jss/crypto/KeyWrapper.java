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

import java.security.spec.AlgorithmParameterSpec;
import java.security.InvalidAlgorithmParameterException;
import java.security.PublicKey;
import java.security.InvalidKeyException;

/**
 * @deprecated Use the JCA interface instead ({@link javax.crypto.Cipher})
 */
public interface KeyWrapper {

    public void initWrap(SymmetricKey wrappingKey,
                            AlgorithmParameterSpec parameters)
        throws InvalidKeyException, InvalidAlgorithmParameterException;

    public void initWrap(PublicKey wrappingKey,
                            AlgorithmParameterSpec parameters)
        throws InvalidKeyException, InvalidAlgorithmParameterException;

    /**
     * For wrapping keys in plaintext.
     */
    public void initWrap()
        throws InvalidKeyException, InvalidAlgorithmParameterException;

    public void initUnwrap(SymmetricKey unwrappingKey,
                            AlgorithmParameterSpec parameters)
        throws InvalidKeyException, InvalidAlgorithmParameterException;

    public void initUnwrap(PrivateKey unwrappingKey,
                            AlgorithmParameterSpec parameters)
        throws InvalidKeyException, InvalidAlgorithmParameterException;

    /**
     * For plaintext-wrapped keys.
     */
    public void initUnwrap()
        throws InvalidKeyException, InvalidAlgorithmParameterException;

    public byte[] wrap(PrivateKey toBeWrapped)
        throws InvalidKeyException, IllegalStateException, TokenException;

    public byte[] wrap(SymmetricKey toBeWrapped)
        throws InvalidKeyException, IllegalStateException, TokenException;

    /**
     * Unwraps a private key, creating a permanent private key object.
     * A permanent private key object resides on a token until it is
     * explicitly deleted from the token.
     *
     * @param publicKey Used to calculate the key identifier that must be stored
     *  with the private key. Must be a <code>RSAPublicKey</code> or a
     *  <code>DSAPublicKey</code>.
     * @exception InvalidKeyException If the type of the public key does not
     *  match the type of the private key to be unwrapped.
     */
    public PrivateKey unwrapPrivate(byte[] wrapped, PrivateKey.Type type,
        PublicKey publicKey)
        throws TokenException, InvalidKeyException, IllegalStateException;

    /**
     * Unwraps a private key, creating a temporary private key object.
     * A temporary
     * private key is one that does not permanently reside on a token.
     * As soon as it is garbage-collected, it is gone forever.
     *
     * @param publicKey Used to calculate the key identifier that must be stored
     *  with the private key. Must be a <code>RSAPublicKey</code> or a
     *  <code>DSAPublicKey</code>.
     * @exception InvalidKeyException If the type of the public key does not
     *  match the type of the private key to be unwrapped.
     */
    public PrivateKey unwrapTemporaryPrivate(byte[] wrapped,
        PrivateKey.Type type, PublicKey publicKey)
        throws TokenException, InvalidKeyException, IllegalStateException;

    /**
     * @param keyLength The expected length of the key in bytes.  This is 
     *   only used for variable-length keys (RC4) and non-padding
     *   algorithms. Otherwise, it can be set to anything(like 0).
     * @param usage The operation the key will be used for after it is
     *   unwrapped. You have to specify this so that the key can be properly
     *   marked with the operation it supports. Some PKCS #11 tokens require
     *   that a key be marked for an operation before it can perform that
     *   operation.
     */
    public SymmetricKey unwrapSymmetric(byte[] wrapped, SymmetricKey.Type type,
        SymmetricKey.Usage usage, int keyLength)
        throws TokenException, IllegalStateException,
            InvalidAlgorithmParameterException;

    /**
     * Unwraps a key and allows it to be used for all operations.
     * @param keyLength The expected length of the key in bytes.  This is 
     *   only used for variable-length keys (RC4) and non-padding
     *   algorithms. Otherwise, it can be set to anything(like 0).
     */
    public SymmetricKey unwrapSymmetric(byte[] wrapped, SymmetricKey.Type type,
        int keyLength)
        throws TokenException, IllegalStateException,
            InvalidAlgorithmParameterException;

}
