/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.jpake.stage;

import java.io.IOException;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;

import org.json.simple.parser.JSONParser;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.jpake.JPakeClient;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.BaseResourceDelegate;
import org.mozilla.gecko.sync.net.SyncResponse;
import org.mozilla.gecko.sync.setup.Constants;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;
import ch.boye.httpclientandroidlib.message.BasicHeader;

public class GetChannelStage extends JPakeStage {

  private interface GetChannelStageDelegate {
    public void handleSuccess(String channel);
    public void handleFailure(String error);
    public void handleError(Exception e);
  }

  @Override
  public void execute(final JPakeClient jClient) {
    Logger.debug(LOG_TAG, "Getting channel.");

    // Make delegate to handle responses and propagate them to JPakeClient.
    GetChannelStageDelegate callbackDelegate = new GetChannelStageDelegate() {

      @Override
      public void handleSuccess(String channel) {
        if (jClient.finished) {
          Logger.debug(LOG_TAG, "Finished; returning.");
          return;
        }

        jClient.channelUrl = jClient.jpakeServer + channel;
        Logger.debug(LOG_TAG, "Using channel " + channel);
        jClient.makeAndDisplayPin(channel);

        jClient.runNextStage();
      }

      @Override
      public void handleFailure(String error) {
        Logger.error(LOG_TAG, "Got HTTP failure: " + error);
        jClient.abort(error);
      }

      @Override
      public void handleError(Exception e) {
        Logger.error(LOG_TAG, "Threw HTTP exception.", e);
        jClient.abort(Constants.JPAKE_ERROR_CHANNEL);
      }
    };

    try {
      makeChannelRequest(callbackDelegate, jClient.jpakeServer + "new_channel", jClient.clientId);
    } catch (URISyntaxException e) {
      Logger.error(LOG_TAG, "Incorrect URI syntax.", e);
      jClient.abort(Constants.JPAKE_ERROR_CHANNEL);
      return;
    } catch (Exception e) {
      Logger.error(LOG_TAG, "Unexpected exception.", e);
      jClient.abort(Constants.JPAKE_ERROR_CHANNEL);
      return;
    }
  }

  private void makeChannelRequest(final GetChannelStageDelegate callbackDelegate, String getChannelUrl, final String clientId) throws URISyntaxException {
    final BaseResource httpResource = new BaseResource(getChannelUrl);
    httpResource.delegate = new BaseResourceDelegate(httpResource) {

      @Override
      public void addHeaders(HttpRequestBase request, DefaultHttpClient client) {
        request.setHeader(new BasicHeader("X-KeyExchange-Id", clientId));
      }

      @Override
      public void handleHttpResponse(HttpResponse response) {
        try {
          SyncResponse res = new SyncResponse(response);
          String channel = null;
          try {
            String body = res.body();
            channel = (String) new JSONParser().parse(body);
          } catch (Exception e) {
            callbackDelegate.handleError(e);
            return;
          }

          if (channel == null) {
            Logger.warn(LOG_TAG, "Got null channel.");
            callbackDelegate.handleFailure(Constants.JPAKE_ERROR_CHANNEL);
            return;
          }

          // Verify input.
          // From http://docs.services.mozilla.com/keyexchange/apis.html#apis
          // Returns in the response body a JSON-encoded random channel id of N chars from [a-z0-9].
          for (int i = 0; i < channel.length(); i++) {
            char c = channel.charAt(i);
            if (('a' <= c && c <= 'z') || ('0' <= c && c <= '9')) {
              continue;
            }

            Logger.warn(LOG_TAG, "Got bad channel name: " + channel + ".");
            callbackDelegate.handleFailure(Constants.JPAKE_ERROR_CHANNEL);
            return;
          }

          callbackDelegate.handleSuccess(channel);
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

    // Make GET request.
    JPakeClient.runOnThread(new Runnable() {
      @Override
      public void run() {
        httpResource.get();
      }
    });
  }
}
