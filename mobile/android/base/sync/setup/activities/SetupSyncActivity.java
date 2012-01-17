/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Android Sync Client.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chenxia Liu <liuche@mozilla.com>
 *  Richard Newman <rnewman@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.sync.setup.activities;

import org.json.simple.JSONObject;
import org.mozilla.gecko.R;
import org.mozilla.gecko.sync.jpake.JPakeClient;
import org.mozilla.gecko.sync.jpake.JPakeNoActivePairingException;
import org.mozilla.gecko.sync.setup.Constants;

import android.accounts.Account;
import android.accounts.AccountAuthenticatorActivity;
import android.accounts.AccountManager;
import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

public class SetupSyncActivity extends AccountAuthenticatorActivity {
  private final static String LOG_TAG     = "SetupSync";

  private boolean             pairWithPin = false;

  // UI elements for pairing through PIN entry.
  private EditText            row1;
  private EditText            row2;
  private EditText            row3;
  private Button              connectButton;
  private LinearLayout        pinError;

  // UI elements for pairing through PIN generation.
  private TextView            setupTitleView;
  private TextView            setupNoDeviceLinkTitleView;
  private TextView            setupSubtitleView;
  private TextView            pinTextView;
  private JPakeClient         jClient;

  // Android context.
  private AccountManager      mAccountManager;
  private Context             mContext;

  public SetupSyncActivity() {
    super();
    Log.i(LOG_TAG, "SetupSyncActivity constructor called.");
  }

  /** Called when the activity is first created. */
  @Override
  public void onCreate(Bundle savedInstanceState) {
    Log.i(LOG_TAG, "Called SetupSyncActivity.onCreate.");
    super.onCreate(savedInstanceState);

    // Set Activity variables.
    mContext = getApplicationContext();
    Log.d(LOG_TAG, "AccountManager.get(" + mContext + ")");
    mAccountManager = AccountManager.get(mContext);

  }

  @Override
  public void onResume() {
    Log.i(LOG_TAG, "Called SetupSyncActivity.onResume.");
    super.onResume();

    if (!hasInternet()) {
      setContentView(R.layout.sync_setup_nointernet);
      return;
    }
    
    // Check whether Sync accounts exist; if not, display J-PAKE PIN.
    Account[] accts = mAccountManager.getAccountsByType(Constants.ACCOUNTTYPE_SYNC);

    if (accts.length == 0) { // Start J-PAKE for pairing if no accounts present.
      displayReceiveNoPin();
      jClient = new JPakeClient(this);
      jClient.receiveNoPin();
      return;
    }

    // Set layout based on starting Intent.
    Bundle extras = this.getIntent().getExtras();
    if (extras != null) {
      boolean isSetup = extras.getBoolean(Constants.INTENT_EXTRA_IS_SETUP);
      if (!isSetup) {
        pairWithPin = true;
        displayPairWithPin();
        return;
      }
    }
    // Display toast for "Only one account supported."
    Toast toast = Toast.makeText(mContext, R.string.sync_notification_oneaccount, Toast.LENGTH_LONG);
    toast.show();
    finish();
  }

  @Override
  public void onPause() {
    super.onPause();

    if (jClient != null) {
      jClient.abort(Constants.JPAKE_ERROR_USERABORT);
    }
  }

  @Override
  public void onNewIntent(Intent intent) {
    setIntent(intent);
  }


  /* Click Handlers */
  public void manualClickHandler(View target) {
    Intent accountIntent = new Intent(this, AccountActivity.class);
    accountIntent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
    startActivity(accountIntent);
    overridePendingTransition(0, 0);
  }

  public void cancelClickHandler(View target) {
    finish();
  }

  public void connectClickHandler(View target) {
    // Set UI feedback.
    pinError.setVisibility(View.INVISIBLE);
    enablePinEntry(false);
    connectButton.requestFocus();
    activateButton(connectButton, false);

    // Extract PIN.
    String pin = row1.getText().toString();
    pin += row2.getText().toString() + row3.getText().toString();

    // Start J-PAKE.
    jClient = new JPakeClient(this);
    jClient.pairWithPin(pin, false);
  }

  public void showClickHandler(View target) {
    Uri uri = null;
    // TODO: fetch these from fennec
    if (pairWithPin) {
      uri = Uri.parse(Constants.LINK_FIND_CODE);
    } else {
      uri = Uri.parse(Constants.LINK_FIND_ADD_DEVICE);
    }
    startActivity(new Intent(Intent.ACTION_VIEW, uri));
  }

  /* Controller methods */
  public void displayPin(String pin) {
    if (pin == null) {
      Log.w(LOG_TAG, "Asked to display null pin.");
      return;
    }
    // Format PIN for display.
    int charPerLine = pin.length() / 3;
    String prettyPin = pin.substring(0, charPerLine) + "\n";
    prettyPin += pin.substring(charPerLine, 2 * charPerLine) + "\n";
    prettyPin += pin.substring(2 * charPerLine, pin.length());

    final String toDisplay = prettyPin;
    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        TextView view = pinTextView;
        if (view == null) {
          Log.w(LOG_TAG, "Couldn't find view to display PIN.");
          return;
        }
        view.setText(toDisplay);
      }
    });
  }

  public void displayAbort(String error) {
    if (!Constants.JPAKE_ERROR_USERABORT.equals(error) && !hasInternet()) {
      setContentView(R.layout.sync_setup_nointernet);
      return;
    }
    if (pairWithPin) {
      runOnUiThread(new Runnable() {
        @Override
        public void run() {
          enablePinEntry(true);
          row1.setText("");
          row2.setText("");
          row3.setText("");
          row1.requestFocus();

          // Display error.
          pinError.setVisibility(View.VISIBLE);
        }
      });
      return;
    }

    // Start new JPakeClient for restarting J-PAKE.
    Log.d(LOG_TAG, "abort reason: " + error);
    if (!Constants.JPAKE_ERROR_USERABORT.equals(error)) {
      jClient = new JPakeClient(this);
      runOnUiThread(new Runnable() {
        @Override
        public void run() {
          displayReceiveNoPin();
          jClient.receiveNoPin();
        }
      });
    }
  }

  /**
   * Device has finished key exchange, waiting for remote device to set up or
   * link to a Sync account. Display "waiting for other device" dialog.
   */
  @SuppressWarnings("unchecked")
  public void onPaired() {
    if (!pairWithPin) {
      runOnUiThread(new Runnable() {
        @Override
        public void run() {
          setContentView(R.layout.sync_setup_jpake_waiting);
        }
      });
      return;
    }

    // Extract Sync account data.
    Account[] accts = mAccountManager.getAccountsByType(Constants.ACCOUNTTYPE_SYNC);
    if (accts.length == 0) {
      // Error, no account present.
      Log.e(LOG_TAG, "No accounts present.");
      displayAbort(Constants.JPAKE_ERROR_INVALID);
      return;
    }

    // TODO: Single account supported. Create account selection if spec changes.
    Account account = accts[0];
    String username  = account.name;
    String password  = mAccountManager.getPassword(account);
    String syncKey   = mAccountManager.getUserData(account, Constants.OPTION_SYNCKEY);
    String serverURL = mAccountManager.getUserData(account, Constants.OPTION_SERVER);

    JSONObject jAccount = new JSONObject();
    jAccount.put(Constants.JSON_KEY_SYNCKEY,  syncKey);
    jAccount.put(Constants.JSON_KEY_ACCOUNT,  username);
    jAccount.put(Constants.JSON_KEY_PASSWORD, password);
    jAccount.put(Constants.JSON_KEY_SERVER,   serverURL);

    Log.d(LOG_TAG, "Extracted account data: " + jAccount.toJSONString());
    try {
      jClient.sendAndComplete(jAccount);
    } catch (JPakeNoActivePairingException e) {
      Log.e(LOG_TAG, "No active J-PAKE pairing.", e);
      // TODO: some user-visible action!
    }
  }

  /**
   * J-PAKE pairing has started, but when this device has generated the PIN for
   * pairing, does not require UI feedback to user.
   */
  public void onPairingStart() {
    if (pairWithPin) {
      // TODO: add in functionality if/when adding pairWithPIN.
    }
  }

  /**
   * On J-PAKE completion, store the Sync Account credentials sent by other
   * device. Display progress to user.
   *
   * @param jCreds
   */
  public void onComplete(JSONObject jCreds) {
    if (!pairWithPin) {
      String accountName  = (String) jCreds.get(Constants.JSON_KEY_ACCOUNT);
      String password     = (String) jCreds.get(Constants.JSON_KEY_PASSWORD);
      String syncKey      = (String) jCreds.get(Constants.JSON_KEY_SYNCKEY);
      String serverURL    = (String) jCreds.get(Constants.JSON_KEY_SERVER);

      Log.d(LOG_TAG, "Using account manager " + mAccountManager);
      final Intent intent = AccountActivity.createAccount(mContext, mAccountManager,
                                                          accountName,
                                                          syncKey, password, serverURL);
      setAccountAuthenticatorResult(intent.getExtras());

      setResult(RESULT_OK, intent);
    }

    jClient = null; // Sync is set up. Kill reference to JPakeClient object.
    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        displayAccount(true);
      }
    });
  }

  /*
   * Helper functions
   */

  private void activateButton(Button button, boolean toActivate) {
    button.setEnabled(toActivate);
    button.setClickable(toActivate);
  }

  private void enablePinEntry(boolean toEnable) {
    row1.setEnabled(toEnable);
    row2.setEnabled(toEnable);
    row3.setEnabled(toEnable);
  }

  /**
   * Displays Sync account setup completed feedback to user.
   *
   * @param isSetup
   *          boolean for whether success screen is reached during setup
   *          completion, or otherwise.
   */
  private void displayAccount(boolean isSetup) {
    Intent intent = new Intent(mContext, SetupSuccessActivity.class);
    intent.setFlags(Constants.FLAG_ACTIVITY_REORDER_TO_FRONT_NO_ANIMATION);
    intent.putExtra(Constants.INTENT_EXTRA_IS_SETUP, isSetup);
    startActivity(intent);
    finish();
  }

  /**
   * Validate PIN entry fields to check if the three PIN entry fields are all
   * filled in.
   *
   * @return true, if all PIN fields have 4 characters, false otherwise
   */
  private boolean pinEntryCompleted() {
    if (row1.length() == 4 &&
        row2.length() == 4 &&
        row3.length() == 4) {
      return true;
    }
    return false;
  }

  private boolean hasInternet() {
    Log.d(LOG_TAG, "Checking internet connectivity.");
    ConnectivityManager connManager = (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
    NetworkInfo wifi = connManager.getNetworkInfo(ConnectivityManager.TYPE_WIFI);
    NetworkInfo mobile = connManager.getNetworkInfo(ConnectivityManager.TYPE_MOBILE);

    if (wifi.isConnected() || mobile.isConnected()) {
      Log.d(LOG_TAG, "Internet connected.");
      return true;
    }
    return false;
  }

  private void displayPairWithPin() {
    setContentView(R.layout.sync_setup_pair);
    connectButton = (Button) findViewById(R.id.pair_button_connect);
    pinError = (LinearLayout) findViewById(R.id.pair_error);

    row1 = (EditText) findViewById(R.id.pair_row1);
    row2 = (EditText) findViewById(R.id.pair_row2);
    row3 = (EditText) findViewById(R.id.pair_row3);

    row1.addTextChangedListener(new TextWatcher() {
      @Override
      public void afterTextChanged(Editable s) {
        if (s.length() == 4) {
          row2.requestFocus();
        }
        activateButton(connectButton, pinEntryCompleted());
      }

      @Override
      public void beforeTextChanged(CharSequence s, int start, int count,
          int after) {
      }

      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count) {
      }

    });
    row2.addTextChangedListener(new TextWatcher() {
      @Override
      public void afterTextChanged(Editable s) {
        if (s.length() == 4) {
          row3.requestFocus();
        }
        activateButton(connectButton, pinEntryCompleted());
      }

      @Override
      public void beforeTextChanged(CharSequence s, int start, int count,
          int after) {
      }

      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count) {
      }

    });

    row3.addTextChangedListener(new TextWatcher() {
      @Override
      public void afterTextChanged(Editable s) {
        activateButton(connectButton, pinEntryCompleted());
      }

      @Override
      public void beforeTextChanged(CharSequence s, int start, int count,
          int after) {
      }

      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count) {
      }

    });
  }

  private void displayReceiveNoPin() {
    setContentView(R.layout.sync_setup);

    // Set up UI.
    setupTitleView = ((TextView) findViewById(R.id.setup_title));
    setupSubtitleView = (TextView) findViewById(R.id.setup_subtitle);
    setupNoDeviceLinkTitleView = (TextView) findViewById(R.id.link_nodevice);
    pinTextView = ((TextView) findViewById(R.id.text_pin));

    // UI checks.
    if (setupTitleView == null) {
      Log.e(LOG_TAG, "No title view.");
    }
    if (setupSubtitleView == null) {
      Log.e(LOG_TAG, "No subtitle view.");
    }
    if (setupNoDeviceLinkTitleView == null) {
      Log.e(LOG_TAG, "No 'no device' link view.");
    }
  }
}
