/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

import java.security.GeneralSecurityException;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;
import ch.boye.httpclientandroidlib.protocol.BasicHttpContext;

/**
 * An <code>AuthHeaderProvider</code> generates HTTP Authorization headers for
 * HTTP requests.
 */
public interface AuthHeaderProvider {
  /**
   * Generate an HTTP Authorization header.
   *
   * @param request HTTP request.
   * @param context HTTP context.
   * @param client HTTP client.
   * @return HTTP Authorization header.
   * @throws GeneralSecurityException usually wrapping a more specific exception.
   */
  Header getAuthHeader(HttpRequestBase request, BasicHttpContext context, DefaultHttpClient client)
    throws GeneralSecurityException;
}
