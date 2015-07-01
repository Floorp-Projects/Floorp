/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.sync;

import android.app.Activity;
import android.app.IntentService;
import android.content.Intent;
import android.os.Bundle;
import android.os.ResultReceiver;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.background.fxa.oauth.FxAccountAbstractClient;
import org.mozilla.gecko.background.fxa.oauth.FxAccountAbstractClientException;
import org.mozilla.gecko.background.fxa.profile.FxAccountProfileClient10;
import org.mozilla.gecko.sync.ExtendedJSONObject;

import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

public class FxAccountProfileService extends IntentService {
  private static final String LOG_TAG = "FxAccountProfileService";
  private static final Executor EXECUTOR_SERVICE = Executors.newSingleThreadExecutor();
  public static final String KEY_AUTH_TOKEN = "auth_token";
  public static final String KEY_PROFILE_SERVER_URI = "profileServerURI";
  public static final String KEY_RESULT_RECEIVER = "resultReceiver";
  public static final String KEY_RESULT_STRING = "RESULT_STRING";

  public FxAccountProfileService() {
    super("FxAccountProfileService");
  }

  @Override
  protected void onHandleIntent(Intent intent) {
    final String authToken = intent.getStringExtra(KEY_AUTH_TOKEN);
    final String profileServerURI = intent.getStringExtra(KEY_PROFILE_SERVER_URI);
    final ResultReceiver resultReceiver = intent.getParcelableExtra(KEY_RESULT_RECEIVER);

    if (resultReceiver == null) {
      Logger.warn(LOG_TAG, "Result receiver must not be null; ignoring intent.");
      return;
    }

    if (authToken == null || authToken.length() == 0) {
      Logger.warn(LOG_TAG, "Invalid Auth Token");
      sendResult("Invalid Auth Token", resultReceiver, Activity.RESULT_CANCELED);
      return;
    }

    if (profileServerURI == null || profileServerURI.length() == 0) {
      Logger.warn(LOG_TAG, "Invalid profile Server Endpoint");
      sendResult("Invalid profile Server Endpoint", resultReceiver, Activity.RESULT_CANCELED);
      return;
    }

    // This delegate fetches the profile avatar json.
    FxAccountProfileClient10.RequestDelegate<ExtendedJSONObject> delegate = new FxAccountAbstractClient.RequestDelegate<ExtendedJSONObject>() {
      @Override
      public void handleError(Exception e) {
        Logger.error(LOG_TAG, "Error fetching Account profile.", e);
        sendResult("Error fetching Account profile.", resultReceiver, Activity.RESULT_CANCELED);
      }

      @Override
      public void handleFailure(FxAccountAbstractClientException.FxAccountAbstractClientRemoteException e) {
        Logger.warn(LOG_TAG, "Failed to fetch Account profile.", e);
        sendResult("Failed to fetch Account profile.", resultReceiver, Activity.RESULT_CANCELED);
      }

      @Override
      public void handleSuccess(ExtendedJSONObject result) {
        if (result != null){
          FxAccountUtils.pii(LOG_TAG, "Profile server return profile: " + result.toJSONString());
          sendResult(result.toJSONString(), resultReceiver, Activity.RESULT_OK);
        }
      }
    };

    FxAccountProfileClient10 client = new FxAccountProfileClient10(profileServerURI, EXECUTOR_SERVICE);
    try {
      client.profile(authToken, delegate);
    } catch (Exception e) {
      Logger.error(LOG_TAG, "Got exception fetching profile.", e);
      delegate.handleError(e);
    }
  }

  private void sendResult(final String result, final ResultReceiver resultReceiver, final int code) {
    if (resultReceiver != null) {
      final Bundle bundle = new Bundle();
      bundle.putString(KEY_RESULT_STRING, result);
      resultReceiver.send(code, bundle);
    }
  }
}
