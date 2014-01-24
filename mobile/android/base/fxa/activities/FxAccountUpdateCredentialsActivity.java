/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import java.io.UnsupportedEncodingException;
import java.security.GeneralSecurityException;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountClient10.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.background.fxa.FxAccountClient20.LoginResponse;
import org.mozilla.gecko.background.fxa.FxAccountClientException.FxAccountClientRemoteException;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.activities.FxAccountSetupTask.FxAccountSignInTask;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.authenticator.FxAccountAuthenticator;
import org.mozilla.gecko.fxa.login.Engaged;
import org.mozilla.gecko.fxa.login.Separated;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.login.State.StateLabel;

import android.accounts.Account;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ProgressBar;
import android.widget.TextView;

/**
 * Activity which displays a screen for updating the local password.
 */
public class FxAccountUpdateCredentialsActivity extends FxAccountAbstractSetupActivity {
  protected static final String LOG_TAG = FxAccountUpdateCredentialsActivity.class.getSimpleName();

  protected AndroidFxAccount fxAccount;
  protected Separated accountState;

  public FxAccountUpdateCredentialsActivity() {
    // We want to share code with the other setup activities, but this activity
    // doesn't create a new Android Account, it modifies an existing one. If you
    // manage to get an account, and somehow be locked out too, we'll let you
    // update it.
    super(CANNOT_RESUME_WHEN_NO_ACCOUNTS_EXIST);
  }

  /**
   * {@inheritDoc}
   */
  @Override
  public void onCreate(Bundle icicle) {
    Logger.debug(LOG_TAG, "onCreate(" + icicle + ")");

    super.onCreate(icicle);
    setContentView(R.layout.fxaccount_update_credentials);

    emailEdit = (EditText) ensureFindViewById(null, R.id.email, "email edit");
    passwordEdit = (EditText) ensureFindViewById(null, R.id.password, "password edit");
    showPasswordButton = (Button) ensureFindViewById(null, R.id.show_password, "show password button");
    remoteErrorTextView = (TextView) ensureFindViewById(null, R.id.remote_error, "remote error text view");
    button = (Button) ensureFindViewById(null, R.id.button, "update credentials");
    progressBar = (ProgressBar) ensureFindViewById(null, R.id.progress, "progress bar");

    minimumPasswordLength = 1; // Minimal restriction on passwords entered to sign in.
    createButton();
    addListeners();
    updateButtonState();
    createShowPasswordButton();

    emailEdit.setEnabled(false);

    // Not yet implemented.
    // this.launchActivityOnClick(ensureFindViewById(null, R.id.forgot_password_link, "forgot password link"), null);
  }

  @Override
  public void onResume() {
    super.onResume();
    Account accounts[] = FxAccountAuthenticator.getFirefoxAccounts(this);
    if (accounts.length < 1 || accounts[0] == null) {
      Logger.warn(LOG_TAG, "No Android accounts.");
      setResult(RESULT_CANCELED);
      finish();
      return;
    }
    this.fxAccount = new AndroidFxAccount(this, accounts[0]);
    if (fxAccount == null) {
      Logger.warn(LOG_TAG, "Could not get Firefox Account from Android account.");
      setResult(RESULT_CANCELED);
      finish();
      return;
    }
    State state = fxAccount.getState();
    if (state.getStateLabel() != StateLabel.Separated) {
      Logger.warn(LOG_TAG, "Could not get state from Firefox Account.");
      setResult(RESULT_CANCELED);
      finish();
      return;
    }
    this.accountState = (Separated) state;
    emailEdit.setText(fxAccount.getAndroidAccount().name);
  }

  protected class UpdateCredentialsDelegate implements RequestDelegate<LoginResponse> {
    public final String email;
    public final String password;
    public final String serverURI;
    public final byte[] quickStretchedPW;

    public UpdateCredentialsDelegate(String email, String password, String serverURI) throws UnsupportedEncodingException, GeneralSecurityException {
      this.email = email;
      this.password = password;
      this.serverURI = serverURI;
      // XXX This needs to be calculated lazily.
      this.quickStretchedPW = FxAccountUtils.generateQuickStretchedPW(email.getBytes("UTF-8"), password.getBytes("UTF-8"));
    }

    @Override
    public void handleError(Exception e) {
      showRemoteError(e, R.string.fxaccount_update_credentials_unknown_error);
    }

    @Override
    public void handleFailure(FxAccountClientRemoteException e) {
      // TODO On isUpgradeRequired, transition to Doghouse state.
      showRemoteError(e, R.string.fxaccount_update_credentials_unknown_error);
    }

    @Override
    public void handleSuccess(LoginResponse result) {
      Logger.info(LOG_TAG, "Got success signing in.");

      if (fxAccount == null) {
        this.handleError(new IllegalStateException("fxAccount must not be null"));
        return;
      }

      byte[] unwrapkB;
      try {
        unwrapkB = FxAccountUtils.generateUnwrapBKey(quickStretchedPW);
      } catch (Exception e) {
        this.handleError(e);
        return;
      }
      fxAccount.setState(new Engaged(email, result.uid, result.verified, unwrapkB, result.sessionToken, result.keyFetchToken));

      // For great debugging.
      if (FxAccountConstants.LOG_PERSONAL_INFORMATION) {
        fxAccount.dump();
      }

      redirectToActivity(FxAccountStatusActivity.class);
    }
  }

  public void updateCredentials(String email, String password) {
    String serverURI = FxAccountConstants.DEFAULT_IDP_ENDPOINT;
    Executor executor = Executors.newSingleThreadExecutor();
    FxAccountClient20 client = new FxAccountClient20(serverURI, executor);
    try {
      hideRemoteError();
      RequestDelegate<LoginResponse> delegate = new UpdateCredentialsDelegate(email, password, serverURI);
      new FxAccountSignInTask(this, this, email, password, client, delegate).execute();
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Got exception updating credentials for account.", e);
      showRemoteError(e, R.string.fxaccount_update_credentials_unknown_error);
    }
  }

  protected void createButton() {
    button.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        final String email = emailEdit.getText().toString();
        final String password = passwordEdit.getText().toString();
        updateCredentials(email, password);
      }
    });
  }
}
