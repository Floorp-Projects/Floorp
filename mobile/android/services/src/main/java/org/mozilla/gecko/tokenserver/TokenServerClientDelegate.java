/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tokenserver;


public interface TokenServerClientDelegate {
  void handleSuccess(TokenServerToken token);
  void handleFailure(TokenServerException e);
  void handleError(Exception e);

  /**
   * Might be called multiple times, in addition to the other terminating handler methods.
   */
  void handleBackoff(int backoffSeconds);

  public String getUserAgent();
}
