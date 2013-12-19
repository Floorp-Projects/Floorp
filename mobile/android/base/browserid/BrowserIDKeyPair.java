/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.browserid;

public class BrowserIDKeyPair {
  protected final SigningPrivateKey privateKey;
  protected final VerifyingPublicKey publicKey;

  public BrowserIDKeyPair(SigningPrivateKey privateKey, VerifyingPublicKey publicKey) {
    this.privateKey = privateKey;
    this.publicKey = publicKey;
  }

  public SigningPrivateKey getPrivate() {
    return this.privateKey;
  }

  public VerifyingPublicKey getPublic() {
    return this.publicKey;
  }
}
