/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

/**
 * An <code>AuthHeaderProvider</code> that returns an Authorization header for
 * BrowserID assertions in the format expected by a Mozilla Services Token
 * Server.
 * <p>
 * See <a href="http://docs.services.mozilla.com/token/apis.html">http://docs.services.mozilla.com/token/apis.html</a>.
 */
public class BrowserIDAuthHeaderProvider extends AbstractBearerTokenAuthHeaderProvider {
  public BrowserIDAuthHeaderProvider(String assertion) {
    super(assertion);
  }

  @Override
  protected String getPrefix() {
    return "BrowserID";
  }
}
