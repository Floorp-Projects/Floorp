/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.browserid;

import org.mozilla.gecko.sync.ExtendedJSONObject;

public class BrowserIDKeyPair {
  public static final String JSON_KEY_PRIVATEKEY = "privateKey";
  public static final String JSON_KEY_PUBLICKEY = "publicKey";

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

  public ExtendedJSONObject toJSONObject() {
    ExtendedJSONObject o = new ExtendedJSONObject();
    o.put(JSON_KEY_PRIVATEKEY, privateKey.toJSONObject());
    o.put(JSON_KEY_PUBLICKEY, publicKey.toJSONObject());
    return o;
  }
}
