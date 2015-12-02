/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.browserid;

import java.security.GeneralSecurityException;

import org.mozilla.gecko.sync.ExtendedJSONObject;

public interface SigningPrivateKey {
  /**
   * Return the JSON Web Token "alg" header corresponding to this private key.
   * <p>
   * The header is used when formatting web tokens, and generally denotes the
   * algorithm and an ad-hoc encoding of the key size.
   *
   * @return header.
   */
  public String getAlgorithm();

  /**
   * Generate a JSON representation of a private key.
   * <p>
   * <b>This should only be used for debugging. No private keys should go over
   * the wire at any time.</b>
   *
   * @param privateKey
   *          to represent.
   * @return JSON representation.
   */
  public ExtendedJSONObject toJSONObject();

  /**
   * Sign a message.
   * @param message to sign.
   * @return signature.
   * @throws GeneralSecurityException
   */
  public byte[] signMessage(byte[] message) throws GeneralSecurityException;
}
