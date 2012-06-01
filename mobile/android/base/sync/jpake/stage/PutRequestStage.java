/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.jpake.stage;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;
import java.util.Timer;
import java.util.TimerTask;

import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.jpake.JPakeClient;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.Resource;
import org.mozilla.gecko.sync.net.SyncResourceDelegate;
import org.mozilla.gecko.sync.setup.Constants;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;
import ch.boye.httpclientandroidlib.message.BasicHeader;

public class PutRequestStage extends JPakeStage {

  private interface PutRequestStageDelegate {
    public void handleSuccess(HttpResponse response);
    public void handleFailure(String error);
    public void handleError(Exception e);
  };

  @Override
  public void execute(final JPakeClient jClient) {
    Logger.debug(LOG_TAG, "Upload message.");

    // Create delegate.
    final PutRequestStageDelegate callbackDelegate = new PutRequestStageDelegate() {

      @Override
      public void handleSuccess(HttpResponse response) {
        TimerTask runNextStage = new TimerTask() {
          @Override
          public void run() {
            jClient.runNextStage();
          }
        };
        Timer timer = new Timer();

        Logger.debug(LOG_TAG, "Pause for 2 * pollInterval before continuing.");
        // There's no point in returning early here since the next step will
        // always be a GET, so let's pause for twice the poll interval.
        timer.schedule(runNextStage, 2 * jClient.jpakePollInterval);
      }

      @Override
      public void handleFailure(String error) {
        Logger.error(LOG_TAG, "Got HTTP failure: " + error);
        jClient.abort(error);
      }

      @Override
      public void handleError(Exception e) {
        Logger.error(LOG_TAG, "HTTP exception.", e);
        jClient.abort(Constants.JPAKE_ERROR_NETWORK);
      }
    };

    // Create PUT request.
    Resource putRequest;
    try {
      putRequest = createPutRequest(callbackDelegate, jClient);
    } catch (URISyntaxException e) {
      Logger.error(LOG_TAG, "URISyntaxException", e);
      jClient.abort(Constants.JPAKE_ERROR_CHANNEL);
      return;
    }

    try {
      putRequest.put(JPakeClient.jsonEntity(jClient.jOutgoing.object));
    } catch (UnsupportedEncodingException e) {
      Logger.error(LOG_TAG, "UnsupportedEncodingException", e);
      jClient.abort(Constants.JPAKE_ERROR_INTERNAL);
      return;
    }
    Logger.debug(LOG_TAG, "Outgoing message: " + jClient.jOutgoing.toJSONString());
  }

  private Resource createPutRequest(final PutRequestStageDelegate callbackDelegate, final JPakeClient jpakeClient) throws URISyntaxException {
    BaseResource httpResource = new BaseResource(jpakeClient.channelUrl);
    httpResource.delegate = new SyncResourceDelegate(httpResource) {

      @Override
      public void addHeaders(HttpRequestBase request, DefaultHttpClient client) {
        request.setHeader(new BasicHeader("X-KeyExchange-Id", jpakeClient.clientId));
        if (jpakeClient.theirEtag != null) {
          request.setHeader(new BasicHeader("If-Match", jpakeClient.theirEtag));
        } else {
          request.setHeader(new BasicHeader("If-None-Match", "*"));
        }
      }

      @Override
      public void handleHttpResponse(HttpResponse response) {
        try {
          int statusCode = response.getStatusLine().getStatusCode();
          switch (statusCode) {
          case 200:
            Header etagHeader = response.getFirstHeader("etag");
            if (etagHeader == null) {
              Logger.error(LOG_TAG, "Server did not supply ETag.");
              callbackDelegate.handleFailure(Constants.JPAKE_ERROR_SERVER);
              return;
            }
            jpakeClient.myEtag = etagHeader.getValue();
            callbackDelegate.handleSuccess(response);
            break;
          default:
            Logger.error(LOG_TAG, "Could not upload data. Server responded with HTTP " + statusCode);
            callbackDelegate.handleFailure(Constants.JPAKE_ERROR_SERVER);
          }
        } finally {
          BaseResource.consumeEntity(response);
        }
      }

      @Override
      public void handleHttpProtocolException(ClientProtocolException e) {
        callbackDelegate.handleError(e);
      }

      @Override
      public void handleHttpIOException(IOException e) {
        callbackDelegate.handleError(e);
      }

      @Override
      public void handleTransportException(GeneralSecurityException e) {
        callbackDelegate.handleError(e);
      }

      @Override
      public int connectionTimeout() {
        return JPakeClient.REQUEST_TIMEOUT;
      }

    };
    return httpResource;
  }
}
