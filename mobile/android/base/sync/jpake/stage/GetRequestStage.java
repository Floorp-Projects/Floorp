/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.jpake.stage;

import java.io.IOException;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;
import java.util.Timer;
import java.util.TimerTask;

import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.jpake.JPakeClient;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.BaseResourceDelegate;
import org.mozilla.gecko.sync.net.Resource;
import org.mozilla.gecko.sync.net.SyncResponse;
import org.mozilla.gecko.sync.setup.Constants;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;
import ch.boye.httpclientandroidlib.message.BasicHeader;

public class GetRequestStage extends JPakeStage {

  private Timer timerScheduler = new Timer();
  private int pollTries;
  private GetStepTimerTask getStepTimerTask;

  private interface GetRequestStageDelegate {
    public void handleSuccess(HttpResponse response);
    public void handleFailure(String error);
    public void handleError(Exception e);
  }

  @Override
  public void execute(final JPakeClient jClient) {
    Logger.debug(LOG_TAG, "Retrieving next message.");

    final GetRequestStageDelegate callbackDelegate = new GetRequestStageDelegate() {

      @Override
      public void handleSuccess(HttpResponse response) {
        if (jClient.finished) {
          Logger.debug(LOG_TAG, "Finished; returning.");
          return;
        }
        SyncResponse res = new SyncResponse(response);

        Header etagHeader = response.getFirstHeader("etag");
        if (etagHeader == null) {
          Logger.error(LOG_TAG, "Server did not supply ETag.");
          jClient.abort(Constants.JPAKE_ERROR_SERVER);
          return;
        }

        jClient.theirEtag = etagHeader.getValue();
        try {
          jClient.jIncoming = res.jsonObjectBody();
        } catch (Exception e) {
          Logger.error(LOG_TAG, "Illegal state.", e);
          jClient.abort(Constants.JPAKE_ERROR_INVALID);
          return;
        }
        Logger.debug(LOG_TAG, "incoming message: " + jClient.jIncoming.toJSONString());

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
        jClient.abort(Constants.JPAKE_ERROR_NETWORK);
      }
    };

    Resource httpRequest;
    try {
      httpRequest = createGetRequest(callbackDelegate, jClient);
    } catch (URISyntaxException e) {
      Logger.error(LOG_TAG, "Incorrect URI syntax.", e);
      jClient.abort(Constants.JPAKE_ERROR_INVALID);
      return;
    }

    Logger.debug(LOG_TAG, "Scheduling GET request.");
    getStepTimerTask = new GetStepTimerTask(httpRequest);
    timerScheduler.schedule(getStepTimerTask, jClient.jpakePollInterval);
  }

  private Resource createGetRequest(final GetRequestStageDelegate callbackDelegate, final JPakeClient jpakeClient) throws URISyntaxException {
    BaseResource httpResource = new BaseResource(jpakeClient.channelUrl);
    httpResource.delegate = new BaseResourceDelegate(httpResource) {

      @Override
      public void addHeaders(HttpRequestBase request, DefaultHttpClient client) {
        request.setHeader(new BasicHeader("X-KeyExchange-Id", jpakeClient.clientId));
        if (jpakeClient.myEtag != null) {
          request.setHeader(new BasicHeader("If-None-Match", jpakeClient.myEtag));
        }
      }

      @Override
      public void handleHttpResponse(HttpResponse response) {
        try {
          int statusCode = response.getStatusLine().getStatusCode();
          switch (statusCode) {
          case 200:
            jpakeClient.pollTries = 0; // Reset pollTries for next GET.
            callbackDelegate.handleSuccess(response);
            break;
          case 304:
            Logger.debug(LOG_TAG, "Channel hasn't been updated yet. Will try again later");
            if (pollTries >= jpakeClient.jpakeMaxTries) {
              Logger.error(LOG_TAG, "Tried for " + pollTries + " times, maxTries " + jpakeClient.jpakeMaxTries + ", aborting");
              callbackDelegate.handleFailure(Constants.JPAKE_ERROR_TIMEOUT);
              break;
            }
            jpakeClient.pollTries += 1;
            if (!jpakeClient.finished) {
              Logger.debug(LOG_TAG, "Scheduling next GET request.");
              scheduleGetRequest(jpakeClient.jpakePollInterval, jpakeClient);
            } else {
              Logger.debug(LOG_TAG, "Resetting pollTries");
              jpakeClient.pollTries = 0;
            }
            break;
          case 404:
            Logger.error(LOG_TAG, "No data found in channel.");
            callbackDelegate.handleFailure(Constants.JPAKE_ERROR_NODATA);
            break;
          case 412: // "Precondition failed"
            Logger.debug(LOG_TAG, "Message already replaced on server by other party.");
            callbackDelegate.handleSuccess(response);
            break;
          default:
            Logger.error(LOG_TAG, "Could not retrieve data. Server responded with HTTP " + statusCode);
            callbackDelegate.handleFailure(Constants.JPAKE_ERROR_SERVER);
            break;
          }
        } finally {
          // Clean up.
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

  /**
   * TimerTask for use with delayed GET requests.
   *
   */
  public class GetStepTimerTask extends TimerTask {
    private Resource request;

    public GetStepTimerTask(Resource request) {
      this.request = request;
    }

    @Override
    public void run() {
      request.get();
    }
  }

  /*
   * Helper method to schedule a GET request with some delay.
   * Basically, run another GetRequestStage.
   */
  private void scheduleGetRequest(int delay, final JPakeClient jClient) {
    timerScheduler.schedule(new TimerTask() {

      @Override
      public void run() {
        new GetRequestStage().execute(jClient);
      }
    }, delay);
  }

}
