/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;

/**
 * Shared abstract class for resource delegate that use the same timeouts
 * and no credentials.
 *
 * @author rnewman
 *
 */
public abstract class SyncResourceDelegate implements ResourceDelegate {
  public static int connectionTimeoutInMillis = 1000 * 30;     // Wait 30s for a connection to open.
  public static int socketTimeoutInMillis     = 1000 * 5 * 60; // Wait 5 minutes for data.

  protected Resource resource;
  public SyncResourceDelegate(Resource resource) {
    this.resource = resource;
  }

  @Override
  public int connectionTimeout() {
    return connectionTimeoutInMillis;
  }

  @Override
  public int socketTimeout() {
    return socketTimeoutInMillis;
  }

  @Override
  public String getCredentials() {
    return null;
  }

  @Override
  public void addHeaders(HttpRequestBase request, DefaultHttpClient client) {
  }
}
