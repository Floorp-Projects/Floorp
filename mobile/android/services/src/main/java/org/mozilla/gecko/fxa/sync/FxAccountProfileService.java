/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.sync;

import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.ResultReceiver;
import android.support.annotation.NonNull;
import android.support.v4.app.JobIntentService;

import org.mozilla.gecko.JobIdsConstants;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.background.fxa.oauth.FxAccountAbstractClient;
import org.mozilla.gecko.background.fxa.oauth.FxAccountAbstractClientException;
import org.mozilla.gecko.background.fxa.profile.FxAccountProfileClient10;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.sync.ExtendedJSONObject;

import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

public class FxAccountProfileService extends JobIntentService {
  private static final String LOG_TAG = "FxAccountProfileService";
  private static final Executor EXECUTOR_SERVICE = Executors.newSingleThreadExecutor();
  private static final String KEY_AUTH_TOKEN = "auth_token";
  private static final String KEY_PROFILE_SERVER_URI = "profileServerURI";
  private static final String KEY_RESULT_RECEIVER = "resultReceiver";

  public static final String KEY_RESULT_STRING = "RESULT_STRING";

  public static void enqueueWork(@NonNull final Context context, @NonNull final Intent workIntent) {
    enqueueWork(context, FxAccountProfileService.class, JobIdsConstants.getIdForProfileFetchJob(), workIntent);
  }

  public static Intent getProfileFetchingIntent(@NonNull final String authToken,
                                              @NonNull final String profileServerURI,
                                              @NonNull final ResultReceiver callback) {
    Intent intent = new Intent();
    intent.putExtra(FxAccountProfileService.KEY_AUTH_TOKEN, authToken);
    intent.putExtra(FxAccountProfileService.KEY_PROFILE_SERVER_URI, profileServerURI);
    intent.putExtra(FxAccountProfileService.KEY_RESULT_RECEIVER, callback);
    return intent;
  }

  @Override
  protected void onHandleWork(@NonNull Intent intent) {
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

        if (e.isInvalidAuthentication()) {
          // The profile server rejected the cached oauth token! Invalidate it.
          // A new token will be generated upon next request.
          Logger.info(LOG_TAG, "Invalidating oauth token after 401!");
          AccountManager.get(FxAccountProfileService.this).invalidateAuthToken(FxAccountConstants.ACCOUNT_TYPE, authToken);
        }

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
