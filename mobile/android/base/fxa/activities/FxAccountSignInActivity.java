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
import org.mozilla.gecko.background.fxa.FxAccountClientException.FxAccountClientRemoteException;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.activities.FxAccountSetupTask.FxAccountSignInTask;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.Engaged;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.sync.setup.Constants;

import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ProgressBar;
import android.widget.TextView;

/**
 * Activity which displays sign in screen to the user.
 */
public class FxAccountSignInActivity extends FxAccountAbstractSetupActivity {
  protected static final String LOG_TAG = FxAccountSignInActivity.class.getSimpleName();

  private static final int CHILD_REQUEST_CODE = 3;

  /**
   * {@inheritDoc}
   */
  @Override
  public void onCreate(Bundle icicle) {
    Logger.debug(LOG_TAG, "onCreate(" + icicle + ")");

    super.onCreate(icicle);
    setContentView(R.layout.fxaccount_sign_in);

    emailEdit = (EditText) ensureFindViewById(null, R.id.email, "email edit");
    passwordEdit = (EditText) ensureFindViewById(null, R.id.password, "password edit");
    showPasswordButton = (Button) ensureFindViewById(null, R.id.show_password, "show password button");
    remoteErrorTextView = (TextView) ensureFindViewById(null, R.id.remote_error, "remote error text view");
    button = (Button) ensureFindViewById(null, R.id.button, "sign in button");
    progressBar = (ProgressBar) ensureFindViewById(null, R.id.progress, "progress bar");

    minimumPasswordLength = 1; // Minimal restriction on passwords entered to sign in.
    createSignInButton();
    addListeners();
    updateButtonState();
    createShowPasswordButton();

    View signInInsteadLink = ensureFindViewById(null, R.id.create_account_link, "create account instead link");
    signInInsteadLink.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        Intent intent = new Intent(FxAccountSignInActivity.this, FxAccountCreateAccountActivity.class);
        intent.putExtra("email", emailEdit.getText().toString());
        intent.putExtra("password", passwordEdit.getText().toString());
        // Per http://stackoverflow.com/a/8992365, this triggers a known bug with
        // the soft keyboard not being shown for the started activity. Why, Android, why?
        intent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
        startActivityForResult(intent, CHILD_REQUEST_CODE);
      }
    });

    // Only set email/password in onCreate; we don't want to overwrite edited values onResume.
    if (getIntent() != null && getIntent().getExtras() != null) {
      Bundle bundle = getIntent().getExtras();
      emailEdit.setText(bundle.getString("email"));
      passwordEdit.setText(bundle.getString("password"));
    }

    // Not yet implemented.
    // this.launchActivityOnClick(ensureFindViewById(null, R.id.forgot_password_link, "forgot password link"), null);
  }

  /**
   * We might have switched to the CreateAccount activity; if that activity
   * succeeds, feed its result back to the authenticator.
   */
  @Override
  public void onActivityResult(int requestCode, int resultCode, Intent data) {
    Logger.debug(LOG_TAG, "onActivityResult: " + requestCode);
    if (requestCode != CHILD_REQUEST_CODE || resultCode != RESULT_OK) {
      super.onActivityResult(requestCode, resultCode, data);
      return;
    }
    this.setResult(resultCode, data);
    this.finish();
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
      showRemoteError(e, R.string.fxaccount_sign_in_unknown_error);
    }

    @Override
    public void handleFailure(FxAccountClientRemoteException e) {
      showRemoteError(e, R.string.fxaccount_sign_in_unknown_error);
    }

    @Override
    public void handleSuccess(LoginResponse result) {
      Activity activity = FxAccountSignInActivity.this;
      Logger.info(LOG_TAG, "Got success signing in.");

      // We're on the UI thread, but it's okay to create the account here.
      AndroidFxAccount fxAccount;
      try {
        final String profile = Constants.DEFAULT_PROFILE;
        final String tokenServerURI = FxAccountConstants.DEFAULT_TOKEN_SERVER_URI;
        // TODO: This is wasteful.  We should be able to thread these through so they don't get recomputed.
        byte[] quickStretchedPW = FxAccountUtils.generateQuickStretchedPW(email.getBytes("UTF-8"), password.getBytes("UTF-8"));
        byte[] unwrapkB = FxAccountUtils.generateUnwrapBKey(quickStretchedPW);
        State state = new Engaged(email, result.uid, result.verified, unwrapkB, result.sessionToken, result.keyFetchToken);
        fxAccount = AndroidFxAccount.addAndroidAccount(activity, email, password,
            profile,
            serverURI,
            tokenServerURI,
            state);
        if (fxAccount == null) {
          throw new RuntimeException("Could not add Android account.");
        }
      } catch (Exception e) {
        handleError(e);
        return;
      }

      // For great debugging.
      if (FxAccountConstants.LOG_PERSONAL_INFORMATION) {
        fxAccount.dump();
      }

      // The GetStarted activity has called us and needs to return a result to the authenticator.
      final Intent intent = new Intent();
      intent.putExtra(AccountManager.KEY_ACCOUNT_NAME, email);
      intent.putExtra(AccountManager.KEY_ACCOUNT_TYPE, FxAccountConstants.ACCOUNT_TYPE);
      // intent.putExtra(AccountManager.KEY_AUTHTOKEN, accountType);
      setResult(RESULT_OK, intent);
      finish();

      // Show success activity depending on verification status.
      Intent successIntent;
      if (result.verified) {
        successIntent = new Intent(FxAccountSignInActivity.this, FxAccountVerifiedAccountActivity.class);
      } else {
        successIntent = new Intent(FxAccountSignInActivity.this, FxAccountConfirmAccountActivity.class);
        successIntent.putExtra("sessionToken", result.sessionToken);
      }
      successIntent.putExtra("email", email);
      // Per http://stackoverflow.com/a/8992365, this triggers a known bug with
      // the soft keyboard not being shown for the started activity. Why, Android, why?
      successIntent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
      startActivity(successIntent);
    }
  }

  public void signIn(String email, String password) {
    String serverURI = FxAccountConstants.DEFAULT_IDP_ENDPOINT;
    RequestDelegate<LoginResponse> delegate = new SignInDelegate(email, password, serverURI);
    Executor executor = Executors.newSingleThreadExecutor();
    FxAccountClient20 client = new FxAccountClient20(serverURI, executor);
    try {
      hideRemoteError();
      new FxAccountSignInTask(this, this, email, password, client, delegate).execute();
    } catch (Exception e) {
      showRemoteError(e, R.string.fxaccount_sign_in_unknown_error);
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
