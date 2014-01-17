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
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.activities.FxAccountSetupTask.FxAccountSignUpTask;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.authenticator.FxAccountAuthenticator;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

import android.accounts.Account;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;
import ch.boye.httpclientandroidlib.HttpResponse;

/**
 * Activity which displays create account screen to the user.
 */
public class FxAccountCreateAccountActivity extends FxAccountAbstractSetupActivity {
  protected static final String LOG_TAG = FxAccountCreateAccountActivity.class.getSimpleName();

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

    launchActivityOnClick(ensureFindViewById(null, R.id.sign_in_instead_link, "sign in instead link"), FxAccountSignInActivity.class);
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

  protected void createYearEdit() {
    yearEdit.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        final String[] years = new String[20];
        for (int i = 0; i < years.length; i++) {
          years[i] = Integer.toString(2014 - i);
        }

        android.content.DialogInterface.OnClickListener listener = new Dialog.OnClickListener() {
          @Override
          public void onClick(DialogInterface dialog, int which) {
            yearEdit.setText(years[which]);
          }
        };

        AlertDialog dialog = new AlertDialog.Builder(FxAccountCreateAccountActivity.this)
        .setTitle(R.string.fxaccount_when_were_you_born)
        .setItems(years, listener)
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
        account = AndroidFxAccount.addAndroidAccount(activity, email, password,
            serverURI, null, null, false);
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

  public void createAccount(String email, String password) {
    String serverURI = FxAccountConstants.DEFAULT_IDP_ENDPOINT;
    RequestDelegate<String> delegate = new CreateAccountDelegate(email, password, serverURI);
    Executor executor = Executors.newSingleThreadExecutor();
    FxAccountClient20 client = new FxAccountClient20(serverURI, executor);
    try {
      new FxAccountSignUpTask(this, email, password, client, delegate).execute();
    } catch (Exception e) {
      showRemoteError(e);
    }
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
        createAccount(email, password);
      }
    });
  }
}
