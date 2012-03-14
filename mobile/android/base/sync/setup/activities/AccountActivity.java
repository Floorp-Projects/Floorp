/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup.activities;

import java.util.Locale;

import org.mozilla.gecko.R;
import org.mozilla.gecko.sync.setup.Constants;
import org.mozilla.gecko.sync.setup.SyncAccounts;

import android.accounts.AccountAuthenticatorActivity;
import android.accounts.AccountManager;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.EditText;

public class AccountActivity extends AccountAuthenticatorActivity {
  private final static String LOG_TAG        = "AccountActivity";

  private AccountManager      mAccountManager;
  private Context             mContext;
  private String              username;
  private String              password;
  private String              key;
  private String              server;

  // UI elements.
  private EditText            serverInput;
  private EditText            usernameInput;
  private EditText            passwordInput;
  private EditText            synckeyInput;
  private CheckBox            serverCheckbox;
  private Button              connectButton;

  @Override
  public void onCreate(Bundle savedInstanceState) {
    setTheme(R.style.SyncTheme);
    super.onCreate(savedInstanceState);
    setContentView(R.layout.sync_account);
    mContext = getApplicationContext();
    Log.d(LOG_TAG, "AccountManager.get(" + mContext + ")");
    mAccountManager = AccountManager.get(mContext);

    // Find UI elements.
    usernameInput = (EditText) findViewById(R.id.usernameInput);
    passwordInput = (EditText) findViewById(R.id.passwordInput);
    synckeyInput = (EditText) findViewById(R.id.keyInput);
    serverInput = (EditText) findViewById(R.id.serverInput);

    TextWatcher inputValidator = makeInputValidator();

    usernameInput.addTextChangedListener(inputValidator);
    passwordInput.addTextChangedListener(inputValidator);
    synckeyInput.addTextChangedListener(inputValidator);
    serverInput.addTextChangedListener(inputValidator);

    connectButton = (Button) findViewById(R.id.accountConnectButton);
    serverCheckbox = (CheckBox) findViewById(R.id.checkbox_server);

    serverCheckbox.setOnCheckedChangeListener(new OnCheckedChangeListener() {
      @Override
      public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        Log.i(LOG_TAG, "Toggling checkbox: " + isChecked);
        // Hack for pre-3.0 Android: can enter text into disabled EditText.
        if (!isChecked) { // Clear server input.
          serverInput.setVisibility(View.GONE);
          serverInput.setText("");
        } else {
          serverInput.setVisibility(View.VISIBLE);
        }
        // Activate connectButton if necessary.
        activateView(connectButton, validateInputs());
      }
    });
  }

  @Override
  public void onStart() {
    super.onStart();
    // Start with an empty form
    usernameInput.setText("");
    passwordInput.setText("");
    synckeyInput.setText("");
    passwordInput.setText("");
  }

  public void cancelClickHandler(View target) {
    finish();
  }

  /*
   * Get credentials on "Connect" and write to AccountManager, where it can be
   * accessed by Fennec and Sync Service.
   */
  public void connectClickHandler(View target) {
    Log.d(LOG_TAG, "connectClickHandler for view " + target);
    username = usernameInput.getText().toString().toLowerCase(Locale.US);
    password = passwordInput.getText().toString();
    key = synckeyInput.getText().toString();
    if (serverCheckbox.isChecked()) {
      server = serverInput.getText().toString();
    }
    enableCredEntry(false);

    // TODO : Authenticate with Sync Service, once implemented, with
    // onAuthSuccess as callback

    authCallback();
  }

  /* Helper UI functions */
  private void enableCredEntry(boolean toEnable) {
    usernameInput.setEnabled(toEnable);
    passwordInput.setEnabled(toEnable);
    synckeyInput.setEnabled(toEnable);
    if (!toEnable) {
      serverInput.setEnabled(toEnable);
    } else {
      serverInput.setEnabled(serverCheckbox.isChecked());
    }
  }

  private TextWatcher makeInputValidator() {
    return new TextWatcher() {

      @Override
      public void afterTextChanged(Editable s) {
        activateView(connectButton, validateInputs());
      }

      @Override
      public void beforeTextChanged(CharSequence s, int start, int count,
          int after) {
      }

      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count) {
      }
    };
  }

  private boolean validateInputs() {
    if (usernameInput.length() == 0 ||
        passwordInput.length() == 0 ||
        synckeyInput.length() == 0  ||
        (serverCheckbox.isChecked() &&
         serverInput.length() == 0)) {
      return false;
    }
    return true;
  }

  /*
   * Callback that handles auth based on success/failure
   */
  private void authCallback() {
    // Create and add account to AccountManager
    // TODO: only allow one account to be added?
    Log.d(LOG_TAG, "Using account manager " + mAccountManager);
    final Intent intent = SyncAccounts.createAccount(mContext, mAccountManager,
                                        username,
                                        key, password, server);
    setAccountAuthenticatorResult(intent.getExtras());

    // Testing out the authFailure case
    // authFailure();

    // TODO: Currently, we do not actually authenticate username/pass against
    // Moz sync server.

    // Successful authentication result
    setResult(RESULT_OK, intent);

    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        authSuccess();
      }
    });
  }

  @SuppressWarnings("unused")
  private void authFailure() {
    enableCredEntry(true);
    Intent intent = new Intent(mContext, SetupFailureActivity.class);
    intent.setFlags(Constants.FLAG_ACTIVITY_REORDER_TO_FRONT_NO_ANIMATION);
    startActivity(intent);
  }

  private void authSuccess() {
    Intent intent = new Intent(mContext, SetupSuccessActivity.class);
    intent.setFlags(Constants.FLAG_ACTIVITY_REORDER_TO_FRONT_NO_ANIMATION);
    startActivity(intent);
    finish();
  }

  private void activateView(View view, boolean toActivate) {
    view.setEnabled(toActivate);
    view.setClickable(toActivate);
  }
}
