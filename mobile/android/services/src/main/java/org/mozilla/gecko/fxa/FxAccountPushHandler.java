package org.mozilla.gecko.fxa;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.Context;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;

public class FxAccountPushHandler {
    private static final String LOG_TAG = "FxAccountPush";

    private static final String COMMAND_DEVICE_DISCONNECTED = "fxaccounts:device_disconnected";

    // Forbid instantiation
    private FxAccountPushHandler() {}

    public static void handleFxAPushMessage(Context context, Bundle bundle) {
        Log.i(LOG_TAG, "Handling FxA Push Message");
        String rawMessage = bundle.getString("message");
        JSONObject message = null;
        if (!TextUtils.isEmpty(rawMessage)) {
            try {
                message = new JSONObject(bundle.getString("message"));
            } catch (JSONException e) {
                Log.e(LOG_TAG, "Could not parse JSON", e);
                return;
            }
        }
        if (message == null) {
            // An empty body means we should check the verification state of the account (FxA sends this
            // when the account email is verified for example).
            // TODO: We're only registering the push endpoint when we are in the Married state, that's why we're skipping the message :(
            Log.d(LOG_TAG, "Skipping empty message");
            return;
        }
        try {
            String command = message.getString("command");
            JSONObject data = message.getJSONObject("data");
            switch (command) {
                case COMMAND_DEVICE_DISCONNECTED:
                    handleDeviceDisconnection(context, data);
                    break;
                default:
                    Log.d(LOG_TAG, "No handler defined for FxA Push command " + command);
                    break;
            }
        } catch (JSONException e) {
            Log.e(LOG_TAG, "Error while handling FxA push notification", e);
        }
    }

    private static void handleDeviceDisconnection(Context context, JSONObject data) throws JSONException {
        final Account account = FirefoxAccounts.getFirefoxAccount(context);
        if (account == null) {
            Log.e(LOG_TAG, "The account does not exist anymore");
            return;
        }
        final AndroidFxAccount fxAccount = new AndroidFxAccount(context, account);
        if (!fxAccount.getDeviceId().equals(data.getString("id"))) {
            Log.e(LOG_TAG, "The device ID to disconnect doesn't match with the local device ID.\n"
                            + "Local: " + fxAccount.getDeviceId() + ", ID to disconnect: " + data.getString("id"));
            return;
        }
        AccountManager.get(context).removeAccount(account, null, null);
    }
}
