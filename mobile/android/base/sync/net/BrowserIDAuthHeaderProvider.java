/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;
import ch.boye.httpclientandroidlib.message.BasicHeader;
import ch.boye.httpclientandroidlib.protocol.BasicHttpContext;

/**
 * An <code>AuthHeaderProvider</code> that returns an Authorization header for
 * BrowserID assertions in the format expected by a Mozilla Services Token
 * Server.
 * <p>
 * See <a href="http://docs.services.mozilla.com/token/apis.html">http://docs.services.mozilla.com/token/apis.html</a>.
 */
public class BrowserIDAuthHeaderProvider implements AuthHeaderProvider {
  protected final String assertion;

  public BrowserIDAuthHeaderProvider(String assertion) {
    if (assertion == null) {
      throw new IllegalArgumentException("assertion must not be null.");
    }

    this.assertion = assertion;
  }

  @Override
  public Header getAuthHeader(HttpRequestBase request, BasicHttpContext context, DefaultHttpClient client) {
    Header header = new BasicHeader("Authorization", "BrowserID " + assertion);

    return header;
  }
}
