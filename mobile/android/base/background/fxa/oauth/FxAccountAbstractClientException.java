/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.fxa.oauth;

import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

import ch.boye.httpclientandroidlib.HttpResponse;

/**
 * From <a href="https://github.com/mozilla/fxa-auth-server/blob/master/docs/api.md">https://github.com/mozilla/fxa-auth-server/blob/master/docs/api.md</a>.
 */
public class FxAccountAbstractClientException extends Exception {
  private static final long serialVersionUID = 1953459541558266597L;

  public FxAccountAbstractClientException(String detailMessage) {
    super(detailMessage);
  }

  public FxAccountAbstractClientException(Exception e) {
    super(e);
  }

  public static class FxAccountAbstractClientRemoteException extends FxAccountAbstractClientException {
    private static final long serialVersionUID = 1209313149952001097L;

    public final HttpResponse response;
    public final long httpStatusCode;
    public final long apiErrorNumber;
    public final String error;
    public final String message;
    public final ExtendedJSONObject body;

    public FxAccountAbstractClientRemoteException(HttpResponse response, long httpStatusCode, long apiErrorNumber, String error, String message, ExtendedJSONObject body) {
      super(new HTTPFailureException(new SyncStorageResponse(response)));
      if (body == null) {
        throw new IllegalArgumentException("body must not be null");
      }
      this.response = response;
      this.httpStatusCode = httpStatusCode;
      this.apiErrorNumber = apiErrorNumber;
      this.error = error;
      this.message = message;
      this.body = body;
    }

    @Override
    public String toString() {
      return "<FxAccountAbstractClientRemoteException " + this.httpStatusCode + " [" + this.apiErrorNumber + "]: " + this.message + ">";
    }
  }

  public static class FxAccountAbstractClientMalformedResponseException extends FxAccountAbstractClientRemoteException {
    private static final long serialVersionUID = 1209313149952001098L;

    public FxAccountAbstractClientMalformedResponseException(HttpResponse response) {
      super(response, 0, FxAccountOAuthRemoteError.UNKNOWN_ERROR, "Response malformed", "Response malformed", new ExtendedJSONObject());
    }
  }
}
