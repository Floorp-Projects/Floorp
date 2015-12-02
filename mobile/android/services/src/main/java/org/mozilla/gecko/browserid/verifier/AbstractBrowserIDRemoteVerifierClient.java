/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.browserid.verifier;

import java.io.IOException;
import java.net.URI;
import java.security.GeneralSecurityException;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.browserid.verifier.BrowserIDVerifierException.BrowserIDVerifierErrorResponseException;
import org.mozilla.gecko.browserid.verifier.BrowserIDVerifierException.BrowserIDVerifierMalformedResponseException;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.net.BaseResourceDelegate;
import org.mozilla.gecko.sync.net.Resource;
import org.mozilla.gecko.sync.net.SyncResponse;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;

public abstract class AbstractBrowserIDRemoteVerifierClient implements BrowserIDVerifierClient {
  public static final String LOG_TAG = AbstractBrowserIDRemoteVerifierClient.class.getSimpleName();

  protected static class RemoteVerifierResourceDelegate extends BaseResourceDelegate {
    private final BrowserIDVerifierDelegate delegate;

    protected RemoteVerifierResourceDelegate(Resource resource, BrowserIDVerifierDelegate delegate) {
      super(resource);
      this.delegate = delegate;
    }

    @Override
    public String getUserAgent() {
      return null;
    }

    @Override
    public void handleHttpResponse(HttpResponse response) {
      SyncResponse res = new SyncResponse(response);
      int statusCode = res.getStatusCode();
      Logger.debug(LOG_TAG, "Got response with status code " + statusCode + ".");

      if (statusCode != 200) {
        delegate.handleError(new BrowserIDVerifierErrorResponseException("Expected status code 200."));
        return;
      }

      ExtendedJSONObject o = null;
      try {
        o = res.jsonObjectBody();
      } catch (Exception e) {
        delegate.handleError(new BrowserIDVerifierMalformedResponseException(e));
        return;
      }

      String status = o.getString("status");
      if ("failure".equals(status)) {
        delegate.handleFailure(o);
        return;
      }

      if (!("okay".equals(status))) {
        delegate.handleError(new BrowserIDVerifierMalformedResponseException("Expected status okay, got '" + status + "'."));
        return;
      }

      delegate.handleSuccess(o);
    }

    @Override
    public void handleTransportException(GeneralSecurityException e) {
      Logger.warn(LOG_TAG, "Got transport exception.", e);
      delegate.handleError(e);
    }

    @Override
    public void handleHttpProtocolException(ClientProtocolException e) {
      Logger.warn(LOG_TAG, "Got protocol exception.", e);
      delegate.handleError(e);
    }

    @Override
    public void handleHttpIOException(IOException e) {
      Logger.warn(LOG_TAG, "Got IO exception.", e);
      delegate.handleError(e);
    }
  }

  protected final URI verifierUri;

  public AbstractBrowserIDRemoteVerifierClient(URI verifierUri) {
    this.verifierUri = verifierUri;
  }
}
