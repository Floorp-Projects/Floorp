/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

/**
 * An <code>AuthHeaderProvider</code> that returns an Authorization header for
 * Bearer tokens in the format expected by a Mozilla Firefox Accounts Profile Server.
 * <p>
 * See <a href="https://github.com/mozilla/fxa-profile-server/blob/master/docs/API.md">https://github.com/mozilla/fxa-profile-server/blob/master/docs/API.md</a>.
 */
public class BearerAuthHeaderProvider extends AbstractBearerTokenAuthHeaderProvider {
  public BearerAuthHeaderProvider(String token) {
    super(token);
  }

  @Override
  protected String getPrefix() {
    return "Bearer";
  }
}
