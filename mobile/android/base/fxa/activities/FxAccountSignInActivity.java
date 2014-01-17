/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountClient10.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.background.fxa.FxAccountClient20.LoginResponse;
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
 * Activity which displays sign in screen to the user.
 */
public class FxAccountSignInActivity extends FxAccountAbstractSetupActivity {
  protected static final String LOG_TAG = FxAccountSignInActivity.class.getSimpleName();

  /**
   * {@inheritDoc}
   */
  @Override
  public void onCreate(Bundle icicle) {
    Logger.debug(LOG_TAG, "onCreate(" + icicle + ")");

    super.onCreate(icicle);
    setContentView(R.layout.fxaccount_sign_in);

    localErrorTextView = (TextView) ensureFindViewById(null, R.id.local_error, "local error text view");
    emailEdit = (EditText) ensureFindViewById(null, R.id.email, "email edit");
    passwordEdit = (EditText) ensureFindViewById(null, R.id.password, "password edit");
    showPasswordButton = (Button) ensureFindViewById(null, R.id.show_password, "show password button");
    button = (Button) ensureFindViewById(null, R.id.sign_in_button, "sign in button");

    minimumPasswordLength = 1; // Minimal restriction on passwords entered to sign in.
    createSignInButton();
    addListeners();
    updateButtonState();
    createShowPasswordButton();

    this.launchActivityOnClick(ensureFindViewById(null, R.id.create_account_link, "create account instead link"), FxAccountCreateAccountActivity.class);
    // Not yet implemented.
    this.launchActivityOnClick(ensureFindViewById(null, R.id.forgot_password_link, "forgot password link"), null);
  }

  /**
   * {@inheritDoc}
   */
  @Override
  public void onResume() {
    super.onResume();
    if (FxAccountAuthenticator.getFirefoxAccounts(this).length > 0) {
      redirectToActivity(FxAccountStatusActivity.class);
      return;
    }
  }

  protected class SignInDelegate implements RequestDelegate<LoginResponse> {
    public final String email;
    public final String password;
    public final String serverURI;

    public SignInDelegate(String email, String password, String serverURI) {
      this.email = email;
      this.password = password;
      this.serverURI = serverURI;
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
      Activity activity = FxAccountSignInActivity.this;
      Logger.info(LOG_TAG, "Got success signing in.");

      // We're on the UI thread, but it's okay to create the account here.
      Account account;
      try {
        account = AndroidFxAccount.addAndroidAccount(activity, email, password,
            serverURI, result.sessionToken, result.keyFetchToken, result.verified);
        if (account == null) {
          throw new RuntimeException("XXX what?");
        }
      } catch (Exception e) {
        handleError(e);
        return;
      }

      // For great debugging.
      if (FxAccountConstants.LOG_PERSONAL_INFORMATION) {
        new AndroidFxAccount(activity, account).dump();
      }

      Toast.makeText(getApplicationContext(), "Got success creating account.", Toast.LENGTH_LONG).show();
      redirectToActivity(FxAccountStatusActivity.class);
    }
  }

  public void signIn(String email, String password) {
    String serverURI = FxAccountConstants.DEFAULT_IDP_ENDPOINT;
    RequestDelegate<LoginResponse> delegate = new SignInDelegate(email, password, serverURI);
    Executor executor = Executors.newSingleThreadExecutor();
    FxAccountClient20 client = new FxAccountClient20(serverURI, executor);
    try {
      new FxAccountSignInTask(this, email, password, client, delegate).execute();
    } catch (Exception e) {
      showRemoteError(e);
    }
  }

  protected void createSignInButton() {
    button.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        final String email = emailEdit.getText().toString();
        final String password = passwordEdit.getText().toString();
        signIn(email, password);
      }
    });
  }
}
