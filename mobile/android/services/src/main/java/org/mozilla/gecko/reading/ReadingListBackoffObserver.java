/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

import java.util.concurrent.atomic.AtomicLong;

import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.net.HttpResponseObserver;
import org.mozilla.gecko.sync.net.MozResponse;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.methods.HttpUriRequest;

public class ReadingListBackoffObserver implements HttpResponseObserver {
  protected final String host;
  protected final AtomicLong largestBackoffObservedInSeconds = new AtomicLong(-1);

  public ReadingListBackoffObserver(String host) {
    this.host = host;
    Utils.throwIfNull(host);
  }

  @Override
  public void observeHttpResponse(HttpUriRequest request, HttpResponse response) {
    // Ignore non-Reading List storage requests.
    if (!host.equals(request.getURI().getHost())) {
      return;
    }

    final MozResponse res = new MozResponse(response);
    long backoffInSeconds = -1;
    try {
      backoffInSeconds = Math.max(res.backoffInSeconds(), res.retryAfterInSeconds());
    } catch (NumberFormatException e) {
      // Ignore.
    }

    if (backoffInSeconds <= 0) {
      return;
    }

    while (true) {
      long existingBackoff = largestBackoffObservedInSeconds.get();
      if (existingBackoff >= backoffInSeconds) {
        return;
      }
      if (largestBackoffObservedInSeconds.compareAndSet(existingBackoff, backoffInSeconds)) {
        return;
      }
    }
  }
}
