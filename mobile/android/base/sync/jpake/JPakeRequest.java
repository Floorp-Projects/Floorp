/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.jpake;

import java.net.URI;
import java.net.URISyntaxException;

import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.Resource;
import org.mozilla.gecko.sync.net.ResourceDelegate;

import android.util.Log;
import ch.boye.httpclientandroidlib.HttpEntity;

public class JPakeRequest implements Resource {
  private static String LOG_TAG = "JPakeRequest";

  private BaseResource resource;
  public JPakeRequestDelegate delegate;

  public JPakeRequest(String uri, ResourceDelegate delegate) throws URISyntaxException {
    this(new URI(uri), delegate);
  }

  public JPakeRequest(URI uri, ResourceDelegate delegate) {
    this.resource = new BaseResource(uri);
    this.resource.delegate = delegate;
    Log.d(LOG_TAG, "new URI: " + uri);
  }

  @Override
  public void get() {
    this.resource.get();
  }

  @Override
  public void delete() {
    this.resource.delete();
  }

  @Override
  public void post(HttpEntity body) {
    this.resource.post(body);
  }

  @Override
  public void put(HttpEntity body) {
    this.resource.put(body);
  }
}
