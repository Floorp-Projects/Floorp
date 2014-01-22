/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountAgeLockoutHelper;
import org.mozilla.gecko.background.fxa.FxAccountClient10.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.activities.FxAccountSetupTask.FxAccountCreateAccountTask;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.setup.Constants;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import ch.boye.httpclientandroidlib.HttpResponse;

/**
 * Activity which displays create account screen to the user.
 */
public class FxAccountCreateAccountActivity extends FxAccountAbstractSetupActivity {
  protected static final String LOG_TAG = FxAccountCreateAccountActivity.class.getSimpleName();

  private static final int CHILD_REQUEST_CODE = 2;

  protected String[] yearItems;
  protected EditText yearEdit;

  /**
   * {@inheritDoc}
   */
  @Override
  public void onCreate(Bundle icicle) {
    Logger.debug(LOG_TAG, "onCreate(" + icicle + ")");

    super.onCreate(icicle);
    setContentView(R.layout.fxaccount_create_account);

    linkifyTextViews(null, new int[] { R.id.policy });

    localErrorTextView = (TextView) ensureFindViewById(null, R.id.local_error, "local error text view");
    emailEdit = (EditText) ensureFindViewById(null, R.id.email, "email edit");
    passwordEdit = (EditText) ensureFindViewById(null, R.id.password, "password edit");
    showPasswordButton = (Button) ensureFindViewById(null, R.id.show_password, "show password button");
    yearEdit = (EditText) ensureFindViewById(null, R.id.year_edit, "year edit");
    button = (Button) ensureFindViewById(null, R.id.create_account_button, "create account button");

    createCreateAccountButton();
    createYearEdit();
    addListeners();
    updateButtonState();
    createShowPasswordButton();

    View signInInsteadLink = ensureFindViewById(null, R.id.sign_in_instead_link, "sign in instead link");
    signInInsteadLink.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        Intent intent = new Intent(FxAccountCreateAccountActivity.this, FxAccountSignInActivity.class);
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
  }

  /**
   * We might have switched to the SignIn activity; if that activity
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

  protected void createYearEdit() {
    yearItems = getResources().getStringArray(R.array.fxaccount_create_account_ages_array);

    yearEdit.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        android.content.DialogInterface.OnClickListener listener = new Dialog.OnClickListener() {
          @Override
          public void onClick(DialogInterface dialog, int which) {
            yearEdit.setText(yearItems[which]);
            updateButtonState();
          }
        };
        final AlertDialog dialog = new AlertDialog.Builder(FxAccountCreateAccountActivity.this)
        .setTitle(R.string.fxaccount_when_were_you_born)
        .setItems(yearItems, listener)
        .setIcon(R.drawable.fxaccount_icon)
        .create();

        dialog.show();
      }
    });
  }

  protected class CreateAccountDelegate implements RequestDelegate<String> {
    public final String email;
    public final String password;
    public final String serverURI;

    public CreateAccountDelegate(String email, String password, String serverURI) {
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
      handleError(new HTTPFailureException(new SyncStorageResponse(response)));
    }

    @Override
    public void handleSuccess(String result) {
      Activity activity = FxAccountCreateAccountActivity.this;
      Logger.info(LOG_TAG, "Got success creating account.");

      // We're on the UI thread, but it's okay to create the account here.
      Account account;
      try {
        final String profile = Constants.DEFAULT_PROFILE;
        final String tokenServerURI = FxAccountConstants.DEFAULT_TOKEN_SERVER_URI;
        account = AndroidFxAccount.addAndroidAccount(activity, email, password,
            profile,
            serverURI,
            tokenServerURI,
            null, null, false);
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

      // The GetStarted activity has called us and needs to return a result to the authenticator.
      final Intent intent = new Intent();
      intent.putExtra(AccountManager.KEY_ACCOUNT_NAME, email);
      intent.putExtra(AccountManager.KEY_ACCOUNT_TYPE, FxAccountConstants.ACCOUNT_TYPE);
      // intent.putExtra(AccountManager.KEY_AUTHTOKEN, accountType);
      setResult(RESULT_OK, intent);
      finish();

      // Show success activity.
      Intent successIntent = new Intent(FxAccountCreateAccountActivity.this, FxAccountConfirmAccountActivity.class);
      successIntent.putExtra("email", email);
      // Per http://stackoverflow.com/a/8992365, this triggers a known bug with
      // the soft keyboard not being shown for the started activity. Why, Android, why?
      successIntent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
      startActivity(successIntent);
    }
  }

  public void createAccount(String email, String password) {
    String serverURI = FxAccountConstants.DEFAULT_IDP_ENDPOINT;
    RequestDelegate<String> delegate = new CreateAccountDelegate(email, password, serverURI);
    Executor executor = Executors.newSingleThreadExecutor();
    FxAccountClient20 client = new FxAccountClient20(serverURI, executor);
    try {
      new FxAccountCreateAccountTask(this, email, password, client, delegate).execute();
    } catch (Exception e) {
      showRemoteError(e);
    }
  }

  @Override
  protected boolean shouldButtonBeEnabled() {
    return super.shouldButtonBeEnabled() &&
        (yearEdit.length() > 0);
  }

  protected void createCreateAccountButton() {
    button.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        if (!updateButtonState()) {
          return;
        }
        final String email = emailEdit.getText().toString();
        final String password = passwordEdit.getText().toString();
        if (FxAccountAgeLockoutHelper.passesAgeCheck(yearEdit.getText().toString(), yearItems)) {
          FxAccountConstants.pii(LOG_TAG, "Passed age check.");
          createAccount(email, password);
        } else {
          FxAccountConstants.pii(LOG_TAG, "Failed age check!");
          FxAccountAgeLockoutHelper.lockOut(SystemClock.elapsedRealtime());
          setResult(RESULT_CANCELED);
          redirectToActivity(FxAccountCreateAccountNotAllowedActivity.class);
        }
      }
    });
  }
}
