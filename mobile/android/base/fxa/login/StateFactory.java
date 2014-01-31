/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.login;

import java.security.NoSuchAlgorithmException;
import java.security.spec.InvalidKeySpecException;

import org.mozilla.gecko.browserid.RSACryptoImplementation;
import org.mozilla.gecko.fxa.login.State.StateLabel;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.Utils;

public class StateFactory {
  public static State fromJSONObject(StateLabel stateLabel, ExtendedJSONObject o) throws InvalidKeySpecException, NoSuchAlgorithmException, NonObjectJSONException {
    Long version = o.getLong("version");
    if (version == null || version.intValue() != 1) {
      throw new IllegalStateException("version must be 1");
    }
    switch (stateLabel) {
    case Engaged:
      return new Engaged(
          o.getString("email"),
          o.getString("uid"),
          o.getBoolean("verified"),
          Utils.hex2Byte(o.getString("unwrapkB")),
          Utils.hex2Byte(o.getString("sessionToken")),
          Utils.hex2Byte(o.getString("keyFetchToken")));
    case Cohabiting:
      return new Cohabiting(
          o.getString("email"),
          o.getString("uid"),
          Utils.hex2Byte(o.getString("sessionToken")),
          Utils.hex2Byte(o.getString("kA")),
          Utils.hex2Byte(o.getString("kB")),
          RSACryptoImplementation.fromJSONObject(o.getObject("keyPair")));
    case Married:
      return new Married(
          o.getString("email"),
          o.getString("uid"),
          Utils.hex2Byte(o.getString("sessionToken")),
          Utils.hex2Byte(o.getString("kA")),
          Utils.hex2Byte(o.getString("kB")),
          RSACryptoImplementation.fromJSONObject(o.getObject("keyPair")),
          o.getString("certificate"));
    case Separated:
      return new Separated(
          o.getString("email"),
          o.getString("uid"),
          o.getBoolean("verified"));
    case Doghouse:
      return new Doghouse(
          o.getString("email"),
          o.getString("uid"),
          o.getBoolean("verified"));
    default:
      throw new IllegalStateException("unrecognized state label: " + stateLabel);
    }
  }
}
