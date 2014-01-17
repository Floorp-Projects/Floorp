/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.authenticator;

/**
 * Abstraction around things that might need to be signalled to the user via UI,
 * such as:
 * <ul>
 * <li>account not yet verified;</li>
 * <li>account password needs to be updated;</li>
 * <li>account key management required or changed;</li>
 * <li>auth protocol has changed and Firefox needs to be upgraded;</li>
 * </ul>
 * etc.
 * <p>
 * Consumers of this code should differentiate error classes based on the types
 * of the exceptions thrown. Exceptions that do not have special meaning are of
 * type <code>FxAccountLoginException</code> with an appropriate
 * <code>cause</code> inner exception.
 */
public interface FxAccountLoginDelegate {
  public void handleError(FxAccountLoginException e);
  public void handleSuccess(String assertion);
}
