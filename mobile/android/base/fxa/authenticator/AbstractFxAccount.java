/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.authenticator;

import java.security.GeneralSecurityException;

import org.mozilla.gecko.browserid.BrowserIDKeyPair;

/**
 * A representation of a Firefox Account.
 * <p>
 * Keeps track of:
 * <ul>
 * <li>tokens;</li>
 * <li>verification state;</li>
 * <li>auth server managed keys;</li>
 * <li>locally managed key pairs</li>
 * </ul>
 * <p>
 * <code>kA</code> is a <i>recoverable</i> auth server managed key.
 * <code>kB</code> is an <i>unrecoverable</i> auth server managed key. Changing
 * the user's password maintains <code>kA</code> and <code>kB</code>, but
 * resetting the user's password retains only <code>kA</code> (and losees
 * <code>kB</code>).
 * <p>
 * The entropy of <code>kB</code> is partially derived from the server and
 * partially from the user's password. The auth server stores <code>kB</code>
 * remotely, wrapped in a key derived from the user's password. The unwrapping
 * process is implementation specific, but it is expected that the appropriate
 * derivative of the user's password will be stored until
 * <code>setWrappedKb</code> is called, at which point <code>kB</code> will be
 * computed and cached, ready to be returned by <code>getKb</code>.
 */
public interface AbstractFxAccount {
  /**
   * Get the Firefox Account auth server URI that this account login flow should
   * talk to.
   */
  public String getServerURI();

  public byte[] getSessionToken();
  public byte[] getKeyFetchToken();

  public void invalidateSessionToken();
  public void invalidateKeyFetchToken();

  /**
   * Return true if and only if this account is guaranteed to be verified. This
   * is intended to be a local cache of the verified state. Do not query the
   * auth server!
   */
  public boolean isVerified();

  /**
   * Update the account's local cache to reflect that this account is known to
   * be verified.
   */
  public void setVerified();

  public byte[] getKa();
  public void setKa(byte[] kA);

  public byte[] getKb();

  /**
   * The auth server returns <code>kA</code> and <code>wrap(kB)</code> in
   * response to <code>/account/keys</code>. This method accepts that wrapped
   * value and uses whatever (per concrete type) method it can to derive the
   * unwrapped value and cache it for retrieval by <code>getKb</code>.
   * <p>
   * See also {@link AbstractFxAccount}.
   *
   * @param wrappedKb <code>wrap(kB)</code> from auth server response.
   */
  public void setWrappedKb(byte[] wrappedKb);

  BrowserIDKeyPair getAssertionKeyPair() throws GeneralSecurityException;
}
