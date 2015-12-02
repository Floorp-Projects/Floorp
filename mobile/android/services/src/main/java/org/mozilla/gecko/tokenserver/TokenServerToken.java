/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tokenserver;

public class TokenServerToken {
  public final String id;
  public final String key;
  public final String uid;
  public final String endpoint;

  public TokenServerToken(String id, String key, String uid, String endpoint) {
    this.id = id;
    this.key = key;
    this.uid = uid;
    this.endpoint = endpoint;
  }
}