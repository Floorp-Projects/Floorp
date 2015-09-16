/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.authenticator;

import android.accounts.AbstractAccountAuthenticator;
import android.accounts.Account;
import android.accounts.AccountAuthenticatorResponse;
import android.accounts.AccountManager;
import android.accounts.NetworkErrorException;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountClient;
import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.background.fxa.oauth.FxAccountAbstractClient.RequestDelegate;
import org.mozilla.gecko.background.fxa.oauth.FxAccountAbstractClientException.FxAccountAbstractClientRemoteException;
import org.mozilla.gecko.background.fxa.oauth.FxAccountOAuthClient10;
import org.mozilla.gecko.background.fxa.oauth.FxAccountOAuthClient10.AuthorizationResponse;
import org.mozilla.gecko.browserid.BrowserIDKeyPair;
import org.mozilla.gecko.browserid.JSONWebTokenUtils;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.login.FxAccountLoginStateMachine;
import org.mozilla.gecko.fxa.login.FxAccountLoginStateMachine.LoginStateMachineDelegate;
import org.mozilla.gecko.fxa.login.FxAccountLoginTransition.Transition;
import org.mozilla.gecko.fxa.login.Married;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.login.State.StateLabel;
import org.mozilla.gecko.fxa.login.StateFactory;
import org.mozilla.gecko.fxa.sync.FxAccountNotificationManager;
import org.mozilla.gecko.fxa.sync.FxAccountSyncAdapter;

import java.security.NoSuchAlgorithmException;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

public class FxAccountAuthenticator extends AbstractAccountAuthenticator {
  public static final String LOG_TAG = FxAccountAuthenticator.class.getSimpleName();
  public static final int UNKNOWN_ERROR_CODE = 999;

  protected final Context context;
  protected final AccountManager accountManager;

  public FxAccountAuthenticator(Context context) {
    super(context);
    this.context = context;
    this.accountManager = AccountManager.get(context);
  }

  @Override
  public Bundle addAccount(AccountAuthenticatorResponse response,
      String accountType, String authTokenType, String[] requiredFeatures,
      Bundle options)
          throws NetworkErrorException {
    Logger.debug(LOG_TAG, "addAccount");

    // The data associated to each Account should be invalidated when we change
    // the set of Firefox Accounts on the system.
    AndroidFxAccount.invalidateCaches();

    final Bundle res = new Bundle();

    if (!FxAccountConstants.ACCOUNT_TYPE.equals(accountType)) {
      res.putInt(AccountManager.KEY_ERROR_CODE, -1);
      res.putString(AccountManager.KEY_ERROR_MESSAGE, "Not adding unknown account type.");
      return res;
    }

    final Intent intent = new Intent(FxAccountConstants.ACTION_FXA_GET_STARTED);
    res.putParcelable(AccountManager.KEY_INTENT, intent);
    return res;
  }

  @Override
  public Bundle confirmCredentials(AccountAuthenticatorResponse response, Account account, Bundle options)
      throws NetworkErrorException {
    Logger.debug(LOG_TAG, "confirmCredentials");

    return null;
  }

  @Override
  public Bundle editProperties(AccountAuthenticatorResponse response, String accountType) {
    Logger.debug(LOG_TAG, "editProperties");

    return null;
  }

  protected static class Responder {
    final AccountAuthenticatorResponse response;
    final AndroidFxAccount fxAccount;

    public Responder(AccountAuthenticatorResponse response, AndroidFxAccount fxAccount) {
      this.response = response;
      this.fxAccount = fxAccount;
    }

    public void fail(Exception e) {
      Logger.warn(LOG_TAG, "Responding with error!", e);
      fxAccount.releaseSharedAccountStateLock();
      final Bundle result = new Bundle();
      result.putInt(AccountManager.KEY_ERROR_CODE, UNKNOWN_ERROR_CODE);
      result.putString(AccountManager.KEY_ERROR_MESSAGE, e.toString());
      response.onResult(result);
    }

    public void succeed(String authToken) {
      Logger.info(LOG_TAG, "Responding with success!");
      fxAccount.releaseSharedAccountStateLock();
      final Bundle result = new Bundle();
      result.putString(AccountManager.KEY_ACCOUNT_NAME, fxAccount.account.name);
      result.putString(AccountManager.KEY_ACCOUNT_TYPE, fxAccount.account.type);
      result.putString(AccountManager.KEY_AUTHTOKEN, authToken);
      response.onResult(result);
    }
  }

  public abstract static class FxADefaultLoginStateMachineDelegate implements LoginStateMachineDelegate {
    protected final Context context;
    protected final AndroidFxAccount fxAccount;
    protected final Executor executor;
    protected final FxAccountClient client;

    public FxADefaultLoginStateMachineDelegate(Context context, AndroidFxAccount fxAccount) {
      this.context = context;
      this.fxAccount = fxAccount;
      this.executor = Executors.newSingleThreadExecutor();
      this.client = new FxAccountClient20(fxAccount.getAccountServerURI(), executor);
    }

    @Override
    public FxAccountClient getClient() {
      return client;
    }

    @Override
    public long getCertificateDurationInMilliseconds() {
      return 12 * 60 * 60 * 1000;
    }

    @Override
    public long getAssertionDurationInMilliseconds() {
      return 15 * 60 * 1000;
    }

    @Override
    public BrowserIDKeyPair generateKeyPair() throws NoSuchAlgorithmException {
      return StateFactory.generateKeyPair();
    }

    @Override
    public void handleTransition(Transition transition, State state) {
      Logger.info(LOG_TAG, "handleTransition: " + transition + " to " + state.getStateLabel());
    }

    abstract public void handleNotMarried(State notMarried);
    abstract public void handleMarried(Married married);

    @Override
    public void handleFinal(State state) {
      Logger.info(LOG_TAG, "handleFinal: in " + state.getStateLabel());
      fxAccount.setState(state);
      // Update any notifications displayed.
      final FxAccountNotificationManager notificationManager = new FxAccountNotificationManager(FxAccountSyncAdapter.NOTIFICATION_ID);
      notificationManager.update(context, fxAccount);

      if (state.getStateLabel() != StateLabel.Married) {
        handleNotMarried(state);
        return;
      } else {
        handleMarried((Married) state);
      }
    }
  }

  protected void getOAuthToken(final AccountAuthenticatorResponse response, final AndroidFxAccount fxAccount, final String scope) throws NetworkErrorException {
    Logger.info(LOG_TAG, "Fetching oauth token with scope: " + scope);

    final Responder responder = new Responder(response, fxAccount);
    final String oauthServerUri = fxAccount.getOAuthServerURI();

    final String audience;
    try {
      audience = FxAccountUtils.getAudienceForURL(oauthServerUri); // The assertion gets traded in for an oauth bearer token.
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Got exception fetching oauth token.", e);
      responder.fail(e);
      return;
    }

    final FxAccountLoginStateMachine stateMachine = new FxAccountLoginStateMachine();

    stateMachine.advance(fxAccount.getState(), StateLabel.Married, new FxADefaultLoginStateMachineDelegate(context, fxAccount) {
      @Override
      public void handleNotMarried(State state) {
        final String message = "Cannot fetch oauth token from state: " + state.getStateLabel();
        Logger.warn(LOG_TAG, message);
        responder.fail(new RuntimeException(message));
      }

      @Override
      public void handleMarried(final Married married) {
        final String assertion;
        try {
          assertion = married.generateAssertion(audience, JSONWebTokenUtils.DEFAULT_ASSERTION_ISSUER);
          if (FxAccountUtils.LOG_PERSONAL_INFORMATION) {
            JSONWebTokenUtils.dumpAssertion(assertion);
          }
        } catch (Exception e) {
          Logger.warn(LOG_TAG, "Got exception fetching oauth token.", e);
          responder.fail(e);
          return;
        }

        final FxAccountOAuthClient10 oauthClient = new FxAccountOAuthClient10(oauthServerUri, executor);
        Logger.debug(LOG_TAG, "OAuth fetch for scope: " + scope);
        oauthClient.authorization(FxAccountConstants.OAUTH_CLIENT_ID_FENNEC, assertion, null, scope, new RequestDelegate<FxAccountOAuthClient10.AuthorizationResponse>() {
          @Override
          public void handleSuccess(AuthorizationResponse result) {
            Logger.debug(LOG_TAG, "OAuth success.");
            FxAccountUtils.pii(LOG_TAG, "Fetched oauth token: " + result.access_token);
            responder.succeed(result.access_token);
          }

          @Override
          public void handleFailure(FxAccountAbstractClientRemoteException e) {
            Logger.error(LOG_TAG, "OAuth failure.", e);
            if (e.isInvalidAuthentication()) {
              // We were married, generated an assertion, and our assertion was rejected by the
              // oauth client. If it's a 401, we probably have a stale certificate.  If instead of
              // a stale certificate we have bad credentials, the state machine will fail to sign
              // our public key and drive us back to Separated.
              fxAccount.setState(married.makeCohabitingState());
            }
            responder.fail(e);
          }

          @Override
          public void handleError(Exception e) {
            Logger.error(LOG_TAG, "OAuth error.", e);
            responder.fail(e);
          }
        });
      }
    });
  }

  @Override
  public Bundle getAuthToken(final AccountAuthenticatorResponse response,
      final Account account, final String authTokenType, final Bundle options)
          throws NetworkErrorException {
    Logger.debug(LOG_TAG, "getAuthToken: " + authTokenType);

    // If we have a cached authToken, hand it over.
    final String cachedAuthToken = AccountManager.get(context).peekAuthToken(account, authTokenType);
    if (cachedAuthToken != null && !cachedAuthToken.isEmpty()) {
      Logger.info(LOG_TAG, "Return cached token.");
      final Bundle result = new Bundle();
      result.putString(AccountManager.KEY_ACCOUNT_NAME, account.name);
      result.putString(AccountManager.KEY_ACCOUNT_TYPE, account.type);
      result.putString(AccountManager.KEY_AUTHTOKEN, cachedAuthToken);
      return result;
    }

    // If we're asked for an oauth::scope token, try to generate one.
    final String oauthPrefix = "oauth::";
    if (authTokenType != null && authTokenType.startsWith(oauthPrefix)) {
      final String scope = authTokenType.substring(oauthPrefix.length());
      final AndroidFxAccount fxAccount = new AndroidFxAccount(context, account);
      try {
        fxAccount.acquireSharedAccountStateLock(LOG_TAG);
      } catch (InterruptedException e) {
        Logger.warn(LOG_TAG, "Could not acquire account state lock; return error bundle.");
        final Bundle bundle = new Bundle();
        bundle.putInt(AccountManager.KEY_ERROR_CODE, 1);
        bundle.putString(AccountManager.KEY_ERROR_MESSAGE, "Could not acquire account state lock.");
        return bundle;
      }
      getOAuthToken(response, fxAccount, scope);
      return null;
    }

    // Otherwise, fail.
    Logger.warn(LOG_TAG, "Returning error bundle for getAuthToken with unknown token type.");
    final Bundle bundle = new Bundle();
    bundle.putInt(AccountManager.KEY_ERROR_CODE, 2);
    bundle.putString(AccountManager.KEY_ERROR_MESSAGE, "Unknown token type: " + authTokenType);
    return bundle;
  }

  @Override
  public String getAuthTokenLabel(String authTokenType) {
    Logger.debug(LOG_TAG, "getAuthTokenLabel");

    return null;
  }

  @Override
  public Bundle hasFeatures(AccountAuthenticatorResponse response,
      Account account, String[] features) throws NetworkErrorException {
    Logger.debug(LOG_TAG, "hasFeatures");

    return null;
  }

  @Override
  public Bundle updateCredentials(AccountAuthenticatorResponse response,
      Account account, String authTokenType, Bundle options)
          throws NetworkErrorException {
    Logger.debug(LOG_TAG, "updateCredentials");

    return null;
  }

  /**
   * If the account is going to be removed, broadcast an "account deleted"
   * intent. This allows us to clean up the account.
   * <p>
   * It is preferable to receive Android's LOGIN_ACCOUNTS_CHANGED_ACTION broadcast
   * than to create our own hacky broadcast here, but that doesn't include enough
   * information about which Accounts changed to correctly identify whether a Sync
   * account has been removed (when some Firefox channels are installed on the SD
   * card). We can work around this by storing additional state but it's both messy
   * and expensive because the broadcast is noisy.
   * <p>
   * Note that this is <b>not</b> called when an Android Account is blown away
   * due to the SD card being unmounted.
   */
  @Override
  public Bundle getAccountRemovalAllowed(final AccountAuthenticatorResponse response, Account account)
      throws NetworkErrorException {
    Bundle result = super.getAccountRemovalAllowed(response, account);

    if (result == null ||
        !result.containsKey(AccountManager.KEY_BOOLEAN_RESULT) ||
        result.containsKey(AccountManager.KEY_INTENT)) {
      return result;
    }

    final boolean removalAllowed = result.getBoolean(AccountManager.KEY_BOOLEAN_RESULT);
    if (!removalAllowed) {
      return result;
    }

    // Broadcast a message to all Firefox channels sharing this Android
    // Account type telling that this Firefox account has been deleted.
    //
    // Broadcast intents protected with permissions are secure, so it's okay
    // to include private information such as a password.
    final AndroidFxAccount androidFxAccount = new AndroidFxAccount(context, account);
    final Intent intent = androidFxAccount.makeDeletedAccountIntent();
    Logger.info(LOG_TAG, "Account named " + account.name + " being removed; " +
        "broadcasting secure intent " + intent.getAction() + ".");
    context.sendBroadcast(intent, FxAccountConstants.PER_ACCOUNT_TYPE_PERMISSION);

    return result;
  }
}
