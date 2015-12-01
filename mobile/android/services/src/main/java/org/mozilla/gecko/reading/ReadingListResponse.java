/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

import org.mozilla.gecko.sync.net.MozResponse;

import ch.boye.httpclientandroidlib.HttpResponse;

/**
 * A MozResponse that knows about all of the general RL-related headers, like Last-Modified.
 */
public abstract class ReadingListResponse extends MozResponse {
  static interface ResponseFactory<T extends ReadingListResponse> {
    public T getResponse(HttpResponse r);
  }

  public ReadingListResponse(HttpResponse res) {
    super(res);
  }

  public long getLastModified() {
    return getLongHeader("Last-Modified");
  }
}