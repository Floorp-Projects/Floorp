/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.methods.HttpUriRequest;

public interface HttpResponseObserver {
  /**
   * Observe an HTTP response.
   * @param request
   *          The <code>HttpUriRequest<code> that elicited the response.
   *
   * @param response
   *          The <code>HttpResponse</code> to observe.
   */
  public void observeHttpResponse(HttpUriRequest request, HttpResponse response);
}
