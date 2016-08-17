/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa;

import android.content.Context;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.util.Log;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountClient;
import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.background.fxa.FxAccountClient20.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClient20.AccountStatusResponse;
import org.mozilla.gecko.background.fxa.FxAccountClientException.FxAccountClientRemoteException;
import org.mozilla.gecko.background.fxa.FxAccountRemoteError;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.login.State.StateLabel;
import org.mozilla.gecko.fxa.login.TokensAndKeysState;
import org.mozilla.gecko.sync.SharedPreferencesClientsDataDelegate;

import java.io.UnsupportedEncodingException;
import java.security.GeneralSecurityException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/* This class provides a way to register the current device against FxA
 * and also stores the registration details in the Android FxAccount.
 * This should be used in a state where we possess a sessionToken, most likely the Married state.
 */
public class FxAccountDeviceRegistrator {

  public abstract static class RegisterDelegate {
    private boolean allowRecursion = true;
    protected abstract void onComplete(String deviceId);
  }

  public static class InvalidFxAState extends Exception {
    private static final long serialVersionUID = -8537626959811195978L;

    public InvalidFxAState(String message) {
      super(message);
    }
  }

  // The current version of the device registration, we use this to re-register
  // devices after we update what we send on device registration.
  public static final Integer DEVICE_REGISTRATION_VERSION = 1;

  private static final String LOG_TAG = "FxADeviceRegistrator";

  private FxAccountDeviceRegistrator() {}

  public static void register(final AndroidFxAccount fxAccount, final Context context) throws InvalidFxAState {
    register(fxAccount, context, new RegisterDelegate() {
      @Override
      public void onComplete(String deviceId) {}
    });
  }

  /**
   * @throws InvalidFxAState thrown if we're not in a fxa state with a session token
   */
  public static void register(final AndroidFxAccount fxAccount, final Context context,
                              final RegisterDelegate delegate) throws InvalidFxAState {
    final byte[] sessionToken = getSessionToken(fxAccount);

    final FxAccountDevice device;
    String deviceId = fxAccount.getDeviceId();
    String clientName = getClientName(fxAccount, context);
    if (TextUtils.isEmpty(deviceId)) {
      Log.i(LOG_TAG, "Attempting registration for a new device");
      device = FxAccountDevice.forRegister(clientName, "mobile");
    } else {
      Log.i(LOG_TAG, "Attempting registration for an existing device");
      Logger.pii(LOG_TAG, "Device ID: " + deviceId);
      device = FxAccountDevice.forUpdate(deviceId, clientName);
    }

    ExecutorService executor = Executors.newSingleThreadExecutor(); // Not called often, it's okay to spawn another thread
    final FxAccountClient20 fxAccountClient =
        new FxAccountClient20(fxAccount.getAccountServerURI(), executor);
    fxAccountClient.registerOrUpdateDevice(sessionToken, device, new RequestDelegate<FxAccountDevice>() {
      @Override
      public void handleError(Exception e) {
        Log.e(LOG_TAG, "Error while updating a device registration: ", e);
        delegate.onComplete(null);
      }

      @Override
      public void handleFailure(FxAccountClientRemoteException error) {
        Log.e(LOG_TAG, "Error while updating a device registration: ", error);
        if (error.httpStatusCode == 400) {
          if (error.apiErrorNumber == FxAccountRemoteError.UNKNOWN_DEVICE) {
            recoverFromUnknownDevice(fxAccount);
            delegate.onComplete(null);
          } else if (error.apiErrorNumber == FxAccountRemoteError.DEVICE_SESSION_CONFLICT) {
            recoverFromDeviceSessionConflict(error, fxAccountClient, sessionToken, fxAccount,
                context, delegate); // Will call delegate.onComplete
          }
        } else
        if (error.httpStatusCode == 401
            && error.apiErrorNumber == FxAccountRemoteError.INVALID_AUTHENTICATION_TOKEN) {
          handleTokenError(error, fxAccountClient, fxAccount);
          delegate.onComplete(null);
        } else {
          logErrorAndResetDeviceRegistrationVersion(error, fxAccount);
          delegate.onComplete(null);
        }
      }

      @Override
      public void handleSuccess(FxAccountDevice result) {
        Log.i(LOG_TAG, "Device registration complete");
        Logger.pii(LOG_TAG, "Registered device ID: " + result.id);
        fxAccount.setFxAUserData(result.id, DEVICE_REGISTRATION_VERSION);
        delegate.onComplete(result.id);
      }
    });
  }

  private static void logErrorAndResetDeviceRegistrationVersion(
      final FxAccountClientRemoteException error, final AndroidFxAccount fxAccount) {
    Log.e(LOG_TAG, "Device registration failed", error);
    fxAccount.resetDeviceRegistrationVersion();
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

  @Nullable
  private static byte[] getSessionToken(final AndroidFxAccount fxAccount) throws InvalidFxAState {
    State state = fxAccount.getState();
    StateLabel stateLabel = state.getStateLabel();
    if (stateLabel == StateLabel.Cohabiting || stateLabel == StateLabel.Married) {
      TokensAndKeysState tokensAndKeysState = (TokensAndKeysState) state;
      return tokensAndKeysState.getSessionToken();
    }
    throw new InvalidFxAState("Cannot get sessionToken: not in a TokensAndKeysState state");
  }

  private static void handleTokenError(final FxAccountClientRemoteException error,
                                       final FxAccountClient fxAccountClient,
                                       final AndroidFxAccount fxAccount) {
    Log.i(LOG_TAG, "Recovering from invalid token error: ", error);
    logErrorAndResetDeviceRegistrationVersion(error, fxAccount);
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
                                                       final RegisterDelegate delegate) {
    Log.w(LOG_TAG, "device session conflict, attempting to ascertain the correct device id");
    fxAccountClient.deviceList(sessionToken, new RequestDelegate<FxAccountDevice[]>() {
      private void onError() {
        Log.e(LOG_TAG, "failed to recover from device-session conflict");
        logErrorAndResetDeviceRegistrationVersion(error, fxAccount);
        delegate.onComplete(null);
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
            fxAccount.setFxAUserData(device.id, 0); // Reset device registration version
            if (!delegate.allowRecursion) {
              Log.d(LOG_TAG, "Failure to register a device on the second try");
              break;
            }
            delegate.allowRecursion = false; // Make sure we don't fall into an infinite loop
            try {
              register(fxAccount, context, delegate); // Will call delegate.onComplete()
              return;
            } catch (InvalidFxAState e) {
              Log.d(LOG_TAG, "Invalid state when trying to recover from a session conflict ", e);
              break;
            }
          }
        }
        onError();
      }
    });
  }
}
