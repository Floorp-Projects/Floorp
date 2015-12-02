/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.browserid;

import java.security.GeneralSecurityException;

import org.mozilla.gecko.sync.ExtendedJSONObject;


public interface VerifyingPublicKey {
  /**
   * Generate a JSON representation of a public key.
   *
   * @param publicKey
   *          to represent.
   * @return JSON representation.
   */
  public ExtendedJSONObject toJSONObject();

  /**
   * Verify a signature.
   *
   * @param message
   *          to verify signature of.
   * @param signature
   *          to verify.
   * @return true if signature is a signature of message produced by the private
   *         key corresponding to this public key.
   * @throws GeneralSecurityException
   */
  public boolean verifyMessage(byte[] message, byte[] signature) throws GeneralSecurityException;
}
