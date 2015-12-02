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
 * bearer tokens, adding a simple prefix.
 */
public abstract class AbstractBearerTokenAuthHeaderProvider implements AuthHeaderProvider {
  protected final String header;

  public AbstractBearerTokenAuthHeaderProvider(String token) {
    if (token == null) {
      throw new IllegalArgumentException("token must not be null.");
    }

    this.header = getPrefix() + " " + token;
  }

  protected abstract String getPrefix();

  @Override
  public Header getAuthHeader(HttpRequestBase request, BasicHttpContext context, DefaultHttpClient client) {
    return new BasicHeader("Authorization", header);
  }
}
