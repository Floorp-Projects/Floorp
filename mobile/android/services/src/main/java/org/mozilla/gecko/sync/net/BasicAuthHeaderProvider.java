/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.auth.Credentials;
import ch.boye.httpclientandroidlib.auth.UsernamePasswordCredentials;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.impl.auth.BasicScheme;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;
import ch.boye.httpclientandroidlib.protocol.BasicHttpContext;

/**
 * An <code>AuthHeaderProvider</code> that returns an HTTP Basic auth header.
 */
public class BasicAuthHeaderProvider implements AuthHeaderProvider {
  protected final String credentials;

  /**
   * Constructor.
   *
   * @param credentials string in form "user:pass".
   */
  public BasicAuthHeaderProvider(String credentials) {
    this.credentials = credentials;
  }

  /**
   * Constructor.
   *
   * @param user username.
   * @param pass password.
   */
  public BasicAuthHeaderProvider(String user, String pass) {
    this(user + ":" + pass);
  }

  /**
   * Return a Header object representing an Authentication header for HTTP
   * Basic.
   */
  @Override
  public Header getAuthHeader(HttpRequestBase request, BasicHttpContext context, DefaultHttpClient client) {
    Credentials creds = new UsernamePasswordCredentials(credentials);

    // This must be UTF-8 to generate the same Basic Auth headers as desktop for non-ASCII passwords.
    return BasicScheme.authenticate(creds, "UTF-8", false);
  }
}
