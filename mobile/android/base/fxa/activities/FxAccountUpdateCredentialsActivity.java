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
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.activities.FxAccountSetupTask.FxAccountSignInTask;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.authenticator.FxAccountAuthenticator;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

import android.accounts.Account;
import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;
import ch.boye.httpclientandroidlib.HttpResponse;

/**
 * Activity which displays a screen for updating the local password.
 */
public class FxAccountUpdateCredentialsActivity extends FxAccountAbstractSetupActivity {
  protected static final String LOG_TAG = FxAccountUpdateCredentialsActivity.class.getSimpleName();

  protected Account account;

  /**
   * {@inheritDoc}
   */
  @Override
  public void onCreate(Bundle icicle) {
    Logger.debug(LOG_TAG, "onCreate(" + icicle + ")");

    super.onCreate(icicle);
    setContentView(R.layout.fxaccount_update_credentials);

    localErrorTextView = (TextView) ensureFindViewById(null, R.id.local_error, "local error text view");
    emailEdit = (EditText) ensureFindViewById(null, R.id.email, "email edit");
    passwordEdit = (EditText) ensureFindViewById(null, R.id.password, "password edit");
    showPasswordButton = (Button) ensureFindViewById(null, R.id.show_password, "show password button");
    button = (Button) ensureFindViewById(null, R.id.button, "update credentials");

    minimumPasswordLength = 1; // Minimal restriction on passwords entered to sign in.
    createButton();
    addListeners();
    updateButtonState();
    createShowPasswordButton();

    emailEdit.setEnabled(false);

    // Not yet implemented.
    this.launchActivityOnClick(ensureFindViewById(null, R.id.forgot_password_link, "forgot password link"), null);
  }

  @Override
  public void onResume() {
    super.onResume();
    Account accounts[] = FxAccountAuthenticator.getFirefoxAccounts(this);
    if (accounts.length < 1) {
      redirectToActivity(FxAccountGetStartedActivity.class);
      finish();
    }
    account = accounts[0];
    emailEdit.setText(account.name);
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
      this.quickStretchedPW = FxAccountUtils.generateQuickStretchedPW(email.getBytes("UTF-8"), password.getBytes("UTF-8"));
    }

    @Override
    public void handleError(Exception e) {
      showRemoteError(e);
    }

    @Override
    public void handleFailure(int status, HttpResponse response) {
      showRemoteError(new HTTPFailureException(new SyncStorageResponse(response)));
    }

    @Override
    public void handleSuccess(LoginResponse result) {
      Activity activity = FxAccountUpdateCredentialsActivity.this;
      Logger.info(LOG_TAG, "Got success signing in.");

      if (account == null) {
        Logger.warn(LOG_TAG, "account must not be null");
        return;
      }

      AndroidFxAccount fxAccount = new AndroidFxAccount(activity, account);
      // XXX wasteful, should only do this once.
      fxAccount.setQuickStretchedPW(quickStretchedPW);

      // For great debugging.
      if (FxAccountConstants.LOG_PERSONAL_INFORMATION) {
        fxAccount.dump();
      }

      Toast.makeText(getApplicationContext(), "Got success updating account credential.", Toast.LENGTH_LONG).show();
      redirectToActivity(FxAccountStatusActivity.class);
    }
  }

  public void updateCredentials(String email, String password) {
    String serverURI = FxAccountConstants.DEFAULT_IDP_ENDPOINT;
    Executor executor = Executors.newSingleThreadExecutor();
    FxAccountClient20 client = new FxAccountClient20(serverURI, executor);
    try {
      RequestDelegate<LoginResponse> delegate = new UpdateCredentialsDelegate(email, password, serverURI);
      new FxAccountSignInTask(this, email, password, client, delegate).execute();
    } catch (Exception e) {
      showRemoteError(e);
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
