/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa;

import android.content.Context;
import android.content.Intent;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.util.Log;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountClient;
import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.background.fxa.FxAccountClient20.AccountStatusResponse;
import org.mozilla.gecko.background.fxa.FxAccountClient20.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClientException.FxAccountClientRemoteException;
import org.mozilla.gecko.background.fxa.FxAccountRemoteError;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.sync.SharedPreferencesClientsDataDelegate;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

import java.io.UnsupportedEncodingException;
import java.lang.ref.WeakReference;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.security.GeneralSecurityException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/* This class provides a way to register the current device against FxA
 * and also stores the registration details in the Android FxAccount.
 * This should be used in a state where we possess a sessionToken, most likely the Married state.
 */
public class FxAccountDeviceRegistrator implements BundleEventListener {
  private static final String LOG_TAG = "FxADeviceRegistrator";

  // The autopush endpoint expires stale channel subscriptions every 30 days (at a set time during
  // the month, although we don't depend on this). To avoid the FxA service channel silently
  // expiring from underneath us, we unsubscribe and resubscribe every 21 days.
  // Note that this simple schedule means that we might unsubscribe perfectly valid (but old)
  // subscriptions. This will be improved as part of Bug 1345651.
  private static final long TIME_BETWEEN_CHANNEL_REGISTRATION_IN_MILLIS = 21 * 24 * 60 * 60 * 1000L;

  // The current version of the device registration, we use this to re-register
  // devices after we update what we send on device registration.
  public static final Integer DEVICE_REGISTRATION_VERSION = 2;

  private static FxAccountDeviceRegistrator instance;
  private final WeakReference<Context> context;

  private FxAccountDeviceRegistrator(Context appContext) {
    this.context = new WeakReference<Context>(appContext);
  }

  private static FxAccountDeviceRegistrator getInstance(Context appContext) throws ClassNotFoundException, NoSuchMethodException, InvocationTargetException, IllegalAccessException {
    if (instance == null) {
      FxAccountDeviceRegistrator tempInstance = new FxAccountDeviceRegistrator(appContext);
      tempInstance.setupListeners(); // Set up listener for FxAccountPush:Subscribe:Response
      instance = tempInstance;
    }
    return instance;
  }

  public static boolean needToRenewRegistration(final long timestamp) {
    // NB: we're comparing wall clock to wall clock, at different points in time.
    // It's possible that wall clocks have changed, and our comparison will be meaningless.
    // However, this happens in the context of a sync, and we won't be able to sync anyways if our
    // wall clock deviates too much from time on the server.
    return (System.currentTimeMillis() - timestamp) > TIME_BETWEEN_CHANNEL_REGISTRATION_IN_MILLIS;
  }

  public static void register(Context context) {
    Context appContext = context.getApplicationContext();
    try {
      getInstance(appContext).beginRegistration(appContext);
    } catch (Exception e) {
      Log.e(LOG_TAG, "Could not start FxA device registration", e);
    }
  }

  public static void renewRegistration(Context context) {
    Context appContext = context.getApplicationContext();
    try {
      getInstance(appContext).beginRegistrationRenewal(appContext);
    } catch (Exception e) {
      Log.e(LOG_TAG, "Could not start FxA device re-registration", e);
    }
  }

  private void beginRegistration(Context context) {
    // Fire up gecko and send event
    // We create the Intent ourselves instead of using GeckoService.getIntentToCreateServices
    // because we can't import these modules (circular dependency between browser and services)
    final Intent geckoIntent = buildCreatePushServiceIntent(context, "android-fxa-subscribe");
    context.startService(geckoIntent);
    // -> handleMessage()
  }

  private void beginRegistrationRenewal(Context context) {
    // Same as registration, but unsubscribe first to get a fresh subscription.
    final Intent geckoIntent = buildCreatePushServiceIntent(context, "android-fxa-resubscribe");
    context.startService(geckoIntent);
    // -> handleMessage()
  }

  private Intent buildCreatePushServiceIntent(final Context context, final String data) {
    final Intent intent = new Intent();
    intent.setAction("create-services");
    intent.setClassName(context, "org.mozilla.gecko.GeckoService");
    intent.putExtra("category", "android-push-service");
    intent.putExtra("data", data);
    final AndroidFxAccount fxAccount = AndroidFxAccount.fromContext(context);
    intent.putExtra("org.mozilla.gecko.intent.PROFILE_NAME", fxAccount.getProfile());
    return intent;
  }

  @Override
  public void handleMessage(String event, GeckoBundle message, EventCallback callback) {
    if ("FxAccountsPush:Subscribe:Response".equals(event)) {
      doFxaRegistration(message.getBundle("subscription"));
    } else {
      Log.e(LOG_TAG, "No action defined for " + event);
    }
  }

  private void doFxaRegistration(GeckoBundle subscription) {
    final Context context = this.context.get();
    if (this.context == null) {
      throw new IllegalStateException("Application context has been gc'ed");
    }
    doFxaRegistration(context, subscription, true);
  }

  private static void doFxaRegistration(final Context context, final GeckoBundle subscription, final boolean allowRecursion) {
    String pushCallback = subscription.getString("pushCallback");
    String pushPublicKey = subscription.getString("pushPublicKey");
    String pushAuthKey = subscription.getString("pushAuthKey");

    final AndroidFxAccount fxAccount = AndroidFxAccount.fromContext(context);
    if (fxAccount == null) {
      Log.e(LOG_TAG, "AndroidFxAccount is null");
      return;
    }
    final byte[] sessionToken;
    try {
      sessionToken = fxAccount.getState().getSessionToken();
    } catch (State.NotASessionTokenState e) {
      Log.e(LOG_TAG, "Could not get a session token", e);
      return;
    }
    final FxAccountDevice device;
    String deviceId = fxAccount.getDeviceId();
    String clientName = getClientName(fxAccount, context);
    if (TextUtils.isEmpty(deviceId)) {
      Log.i(LOG_TAG, "Attempting registration for a new device");
      device = FxAccountDevice.forRegister(clientName, "mobile", pushCallback, pushPublicKey, pushAuthKey);
    } else {
      Log.i(LOG_TAG, "Attempting registration for an existing device");
      Logger.pii(LOG_TAG, "Device ID: " + deviceId);
      device = FxAccountDevice.forUpdate(deviceId, clientName, pushCallback, pushPublicKey, pushAuthKey);
    }

    ExecutorService executor = Executors.newSingleThreadExecutor(); // Not called often, it's okay to spawn another thread
    final FxAccountClient20 fxAccountClient =
            new FxAccountClient20(fxAccount.getAccountServerURI(), executor);
    fxAccountClient.registerOrUpdateDevice(sessionToken, device, new RequestDelegate<FxAccountDevice>() {
      @Override
      public void handleError(Exception e) {
        Log.e(LOG_TAG, "Error while updating a device registration: ", e);
        fxAccount.setDeviceRegistrationTimestamp(0L);
      }

      @Override
      public void handleFailure(FxAccountClientRemoteException error) {
        Log.e(LOG_TAG, "Error while updating a device registration: ", error);

        fxAccount.setDeviceRegistrationTimestamp(0L);

        if (error.httpStatusCode == 400) {
          if (error.apiErrorNumber == FxAccountRemoteError.UNKNOWN_DEVICE) {
            recoverFromUnknownDevice(fxAccount);
          } else if (error.apiErrorNumber == FxAccountRemoteError.DEVICE_SESSION_CONFLICT) {
            recoverFromDeviceSessionConflict(error, fxAccountClient, sessionToken, fxAccount, context,
                    subscription, allowRecursion);
          }
        } else
        if (error.httpStatusCode == 401
                && error.apiErrorNumber == FxAccountRemoteError.INVALID_AUTHENTICATION_TOKEN) {
          handleTokenError(error, fxAccountClient, fxAccount);
        } else {
          logErrorAndResetDeviceRegistrationVersionAndTimestamp(error, fxAccount);
        }
      }

      @Override
      public void handleSuccess(FxAccountDevice result) {
        Log.i(LOG_TAG, "Device registration complete");
        Logger.pii(LOG_TAG, "Registered device ID: " + result.id);
        fxAccount.setFxAUserData(result.id, DEVICE_REGISTRATION_VERSION, System.currentTimeMillis());
      }
    });
  }

  private static void logErrorAndResetDeviceRegistrationVersionAndTimestamp(
      final FxAccountClientRemoteException error, final AndroidFxAccount fxAccount) {
    Log.e(LOG_TAG, "Device registration failed", error);
    fxAccount.resetDeviceRegistrationVersion();
    fxAccount.setDeviceRegistrationTimestamp(0L);
  }

  @Nullable
  private static String getClientName(final AndroidFxAccount fxAccount, final Context context) {
    try {
      SharedPreferencesClientsDataDelegate clientsDataDelegate =
          new SharedPreferencesClientsDataDelegate(fxAccount.getSyncPrefs(), context);
      return clientsDataDelegate.getClientName();
    } catch (UnsupportedEncodingException | GeneralSecurityException e) {
      Log.e(LOG_TAG, "Unable to get client name.", e);
      return null;
    }
  }

  private static void handleTokenError(final FxAccountClientRemoteException error,
                                       final FxAccountClient fxAccountClient,
                                       final AndroidFxAccount fxAccount) {
    Log.i(LOG_TAG, "Recovering from invalid token error: ", error);
    logErrorAndResetDeviceRegistrationVersionAndTimestamp(error, fxAccount);
    fxAccountClient.accountStatus(fxAccount.getState().uid,
        new RequestDelegate<AccountStatusResponse>() {
      @Override
      public void handleError(Exception e) {
      }

      @Override
      public void handleFailure(FxAccountClientRemoteException e) {
      }

      @Override
      public void handleSuccess(AccountStatusResponse result) {
        State doghouseState = fxAccount.getState().makeDoghouseState();
        if (!result.exists) {
          Log.i(LOG_TAG, "token invalidated because the account no longer exists");
          // TODO: Should be in a "I have an Android account, but the FxA is gone." State.
          // This will do for now..
          fxAccount.setState(doghouseState);
          return;
        }
        Log.e(LOG_TAG, "sessionToken invalid");
        fxAccount.setState(doghouseState);
      }
    });
  }

  private static void recoverFromUnknownDevice(final AndroidFxAccount fxAccount) {
    Log.i(LOG_TAG, "unknown device id, clearing the cached device id");
    fxAccount.setDeviceId(null);
  }

  /**
   * Will call delegate#complete in all cases
   */
  private static void recoverFromDeviceSessionConflict(final FxAccountClientRemoteException error,
                                                       final FxAccountClient fxAccountClient,
                                                       final byte[] sessionToken,
                                                       final AndroidFxAccount fxAccount,
                                                       final Context context,
                                                       final GeckoBundle subscription,
                                                       final boolean allowRecursion) {
    Log.w(LOG_TAG, "device session conflict, attempting to ascertain the correct device id");
    fxAccountClient.deviceList(sessionToken, new RequestDelegate<FxAccountDevice[]>() {
      private void onError() {
        Log.e(LOG_TAG, "failed to recover from device-session conflict");
        logErrorAndResetDeviceRegistrationVersionAndTimestamp(error, fxAccount);
      }

      @Override
      public void handleError(Exception e) {
        onError();
      }

      @Override
      public void handleFailure(FxAccountClientRemoteException e) {
        onError();
      }

      @Override
      public void handleSuccess(FxAccountDevice[] devices) {
        for (FxAccountDevice device : devices) {
          if (device.isCurrentDevice) {
            fxAccount.setFxAUserData(device.id, 0, 0L); // Reset device registration version/timestamp
            if (!allowRecursion) {
              Log.d(LOG_TAG, "Failure to register a device on the second try");
              break;
            }
            doFxaRegistration(context, subscription, false);
            return;
          }
        }
        onError();
      }
    });
  }

  private void setupListeners() throws ClassNotFoundException, NoSuchMethodException,
          InvocationTargetException, IllegalAccessException {
    // We have no choice but to use reflection here, sorry :(
    Class<?> eventDispatcher = Class.forName("org.mozilla.gecko.EventDispatcher");
    Method getInstance = eventDispatcher.getMethod("getInstance");
    Object instance = getInstance.invoke(null);
    Method registerBackgroundThreadListener = eventDispatcher.getMethod("registerBackgroundThreadListener",
            BundleEventListener.class, String[].class);
    registerBackgroundThreadListener.invoke(instance, this, new String[] { "FxAccountsPush:Subscribe:Response" });
  }
}
