/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.bagheera;

import ch.boye.httpclientandroidlib.HttpResponse;

public interface BagheeraRequestDelegate {
  void handleSuccess(int status, String namespace, String id, HttpResponse response);
  void handleError(Exception e);
  void handleFailure(int status, String namespace, HttpResponse response);

  public String getUserAgent();
}
