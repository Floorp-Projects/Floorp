/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.login;

import java.security.NoSuchAlgorithmException;
import java.security.spec.InvalidKeySpecException;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.browserid.BrowserIDKeyPair;
import org.mozilla.gecko.browserid.DSACryptoImplementation;
import org.mozilla.gecko.browserid.RSACryptoImplementation;
import org.mozilla.gecko.fxa.login.State.StateLabel;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.Utils;

/**
 * Create {@link State} instances from serialized representations.
 * <p>
 * Version 1 recognizes 5 state labels (Engaged, Cohabiting, Married, Separated,
 * Doghouse). In the Cohabiting and Married states, the associated key pairs are
 * always RSA key pairs.
 * <p>
 * Version 2 is identical to version 1, except that in the Cohabiting and
 * Married states, the associated keypairs are always DSA key pairs.
 */
public class StateFactory {
  private static final String LOG_TAG = StateFactory.class.getSimpleName();

  private static final int KEY_PAIR_SIZE_IN_BITS_V1 = 1024;

  public static BrowserIDKeyPair generateKeyPair() throws NoSuchAlgorithmException {
    // New key pairs are always DSA.
    return DSACryptoImplementation.generateKeyPair(KEY_PAIR_SIZE_IN_BITS_V1);
  }

  protected static BrowserIDKeyPair keyPairFromJSONObjectV1(ExtendedJSONObject o) throws InvalidKeySpecException, NoSuchAlgorithmException {
    // V1 key pairs are RSA.
    return RSACryptoImplementation.fromJSONObject(o);
  }

  protected static BrowserIDKeyPair keyPairFromJSONObjectV2(ExtendedJSONObject o) throws InvalidKeySpecException, NoSuchAlgorithmException {
    // V2 key pairs are DSA.
    return DSACryptoImplementation.fromJSONObject(o);
  }

  public static State fromJSONObject(StateLabel stateLabel, ExtendedJSONObject o) throws InvalidKeySpecException, NoSuchAlgorithmException, NonObjectJSONException {
    Long version = o.getLong("version");
    if (version == null) {
      throw new IllegalStateException("version must not be null");
    }

    final int v = version.intValue();
    if (v == 3) {
      // The most common case is the most recent version.
      return fromJSONObjectV3(stateLabel, o);
    }
    if (v == 2) {
      return fromJSONObjectV2(stateLabel, o);
    }
    if (v == 1) {
      final State state = fromJSONObjectV1(stateLabel, o);
      return migrateV1toV2(stateLabel, state);
    }
    throw new IllegalStateException("version must be in {1, 2}");
  }

  protected static State fromJSONObjectV1(StateLabel stateLabel, ExtendedJSONObject o) throws InvalidKeySpecException, NoSuchAlgorithmException, NonObjectJSONException {
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
          keyPairFromJSONObjectV1(o.getObject("keyPair")));
    case Married:
      return new Married(
          o.getString("email"),
          o.getString("uid"),
          Utils.hex2Byte(o.getString("sessionToken")),
          Utils.hex2Byte(o.getString("kA")),
          Utils.hex2Byte(o.getString("kB")),
          keyPairFromJSONObjectV1(o.getObject("keyPair")),
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

  /**
   * Exactly the same as {@link fromJSONObjectV1}, except that all key pairs are DSA key pairs.
   */
  protected static State fromJSONObjectV2(StateLabel stateLabel, ExtendedJSONObject o) throws InvalidKeySpecException, NoSuchAlgorithmException, NonObjectJSONException {
    switch (stateLabel) {
    case Cohabiting:
      return new Cohabiting(
          o.getString("email"),
          o.getString("uid"),
          Utils.hex2Byte(o.getString("sessionToken")),
          Utils.hex2Byte(o.getString("kA")),
          Utils.hex2Byte(o.getString("kB")),
          keyPairFromJSONObjectV2(o.getObject("keyPair")));
    case Married:
      return new Married(
          o.getString("email"),
          o.getString("uid"),
          Utils.hex2Byte(o.getString("sessionToken")),
          Utils.hex2Byte(o.getString("kA")),
          Utils.hex2Byte(o.getString("kB")),
          keyPairFromJSONObjectV2(o.getObject("keyPair")),
          o.getString("certificate"));
    default:
      return fromJSONObjectV1(stateLabel, o);
    }
  }

  /**
   * Exactly the same as {@link fromJSONObjectV2}, except that there's a new
   * MigratedFromSyncV11 state.
   */
  protected static State fromJSONObjectV3(StateLabel stateLabel, ExtendedJSONObject o) throws InvalidKeySpecException, NoSuchAlgorithmException, NonObjectJSONException {
    switch (stateLabel) {
    case MigratedFromSync11:
      return new MigratedFromSync11(
          o.getString("email"),
          o.getString("uid"),
          o.getBoolean("verified"),
          o.getString("password"));
    default:
      return fromJSONObjectV2(stateLabel, o);
    }
  }

  protected static void logMigration(State from, State to) {
    if (!FxAccountUtils.LOG_PERSONAL_INFORMATION) {
      return;
    }
    try {
      FxAccountUtils.pii(LOG_TAG, "V1 persisted state is: " + from.toJSONObject().toJSONString());
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Error producing JSON representation of V1 state.", e);
    }
    FxAccountUtils.pii(LOG_TAG, "Generated new V2 state: " + to.toJSONObject().toJSONString());
  }

  protected static State migrateV1toV2(StateLabel stateLabel, State state) throws NoSuchAlgorithmException {
    if (state == null) {
      // This should never happen, but let's be careful.
      Logger.error(LOG_TAG, "Got null state in migrateV1toV2; returning null.");
      return state;
    }

    Logger.info(LOG_TAG, "Migrating V1 persisted State to V2; stateLabel: " + stateLabel);

    // In V1, we use an RSA keyPair. In V2, we use a DSA keyPair. Only
    // Cohabiting and Married states have a persisted keyPair at all; all
    // other states need no conversion at all.
    switch (stateLabel) {
    case Cohabiting: {
      // In the Cohabiting state, we can just generate a new key pair and move on.
      final Cohabiting cohabiting = (Cohabiting) state;
      final BrowserIDKeyPair keyPair = generateKeyPair();
      final State migrated = new Cohabiting(cohabiting.email, cohabiting.uid, cohabiting.sessionToken, cohabiting.kA, cohabiting.kB, keyPair);
      logMigration(cohabiting, migrated);
      return migrated;
    }
    case Married: {
      // In the Married state, we cannot only change the key pair: the stored
      // certificate signs the public key of the now obsolete key pair. We
      // regress to the Cohabiting state; the next time we sync, we should
      // advance back to Married.
      final Married married = (Married) state;
      final BrowserIDKeyPair keyPair = generateKeyPair();
      final State migrated = new Cohabiting(married.email, married.uid, married.sessionToken, married.kA, married.kB, keyPair);
      logMigration(married, migrated);
      return migrated;
    }
    default:
      // Otherwise, V1 and V2 states are identical.
      return state;
    }
  }
}
