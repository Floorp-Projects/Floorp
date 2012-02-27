/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup.activities;

import java.util.HashMap;

import org.json.simple.JSONObject;
import org.mozilla.gecko.R;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.ThreadPool;
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
import android.provider.Settings;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

public class SetupSyncActivity extends AccountAuthenticatorActivity {
  private final static String LOG_TAG = "SetupSync";

  private boolean pairWithPin = false;

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
  private TextView            pinTextView1;
  private TextView            pinTextView2;
  private TextView            pinTextView3;
  private JPakeClient         jClient;

  // Android context.
  private AccountManager      mAccountManager;
  private Context             mContext;

  public SetupSyncActivity() {
    super();
    Logger.info(LOG_TAG, "SetupSyncActivity constructor called.");
  }

  /** Called when the activity is first created. */
  @Override
  public void onCreate(Bundle savedInstanceState) {
    setTheme(R.style.SyncTheme);
    Logger.info(LOG_TAG, "Called SetupSyncActivity.onCreate.");
    super.onCreate(savedInstanceState);

    // Set Activity variables.
    mContext = getApplicationContext();
    Logger.debug(LOG_TAG, "AccountManager.get(" + mContext + ")");
    mAccountManager = AccountManager.get(mContext);
  }

  @Override
  public void onResume() {
    Logger.info(LOG_TAG, "Called SetupSyncActivity.onResume.");
    super.onResume();

    if (!hasInternet()) {
      setContentView(R.layout.sync_setup_nointernet);
      return;
    }

    // Check whether Sync accounts exist; if not, display J-PAKE PIN.
    // Run this on a separate thread to comply with Strict Mode thread policies.
    ThreadPool.run(new Runnable() {
      @Override
      public void run() {
        Account[] accts = mAccountManager.getAccountsByType(Constants.ACCOUNTTYPE_SYNC);
        finishResume(accts);
      }
    });
  }

  public void finishResume(Account[] accts) {
    Logger.debug(LOG_TAG, "Finishing Resume after fetching accounts.");
    if (accts.length == 0) { // Start J-PAKE for pairing if no accounts present.
      Logger.debug(LOG_TAG, "No accounts; starting J-PAKE receiver.");
      displayReceiveNoPin();
      if (jClient != null) {
        // Mark previous J-PAKE as finished. Don't bother propagating back up to this Activity.
        jClient.finished = true;
      }
      jClient = new JPakeClient(this);
      jClient.receiveNoPin();
      return;
    }

    // Set layout based on starting Intent.
    Bundle extras = this.getIntent().getExtras();
    if (extras != null) {
      Logger.debug(LOG_TAG, "SetupSync with extras.");
      boolean isSetup = extras.getBoolean(Constants.INTENT_EXTRA_IS_SETUP);
      if (!isSetup) {
        Logger.debug(LOG_TAG, "Account exists; Pair a Device started.");
        pairWithPin = true;
        displayPairWithPin();
        return;
      }
    }
    
    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        Logger.debug(LOG_TAG, "Only one account supported. Redirecting.");
        // Display toast for "Only one account supported."
        // Redirect to account management.
        Toast toast = Toast.makeText(mContext,
            R.string.sync_notification_oneaccount, Toast.LENGTH_LONG);
        toast.show();

        Intent intent = new Intent(Settings.ACTION_SYNC_SETTINGS);
        intent.setFlags(Constants.FLAG_ACTIVITY_REORDER_TO_FRONT_NO_ANIMATION);
        startActivity(intent);

        finish();
      }
    });
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
    Logger.debug(LOG_TAG, "Connect clicked.");
    // Set UI feedback.
    pinError.setVisibility(View.INVISIBLE);
    enablePinEntry(false);
    connectButton.requestFocus();
    activateButton(connectButton, false);

    // Extract PIN.
    String pin = row1.getText().toString();
    pin += row2.getText().toString() + row3.getText().toString();

    // Start J-PAKE.
    if (jClient != null) {
      // Cancel previous J-PAKE exchange.
      jClient.finished = true;
    }
    jClient = new JPakeClient(this);
    jClient.pairWithPin(pin);
  }

  /**
   * Handler when "Show me how" link is clicked.
   * @param target
   *          View that received the click.
   */
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

  /**
   * Display generated PIN to user.
   * @param pin
   *          12-character string generated for J-PAKE.
   */
  public void displayPin(String pin) {
    if (pin == null) {
      Logger.warn(LOG_TAG, "Asked to display null pin.");
      return;
    }
    // Format PIN for display.
    int charPerLine = pin.length() / 3;
    final String pin1 = pin.substring(0, charPerLine);
    final String pin2 = pin.substring(charPerLine, 2 * charPerLine);
    final String pin3 = pin.substring(2 * charPerLine, pin.length());

    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        TextView view1 = pinTextView1;
        TextView view2 = pinTextView2;
        TextView view3 = pinTextView3;
        if (view1 == null || view2 == null || view3 == null) {
          Logger.warn(LOG_TAG, "Couldn't find view to display PIN.");
          return;
        }
        view1.setText(pin1);
        view2.setText(pin2);
        view3.setText(pin3);
      }
    });
  }

  /**
   * Abort current J-PAKE pairing. Clear forms/restart pairing.
   * @param error
   */
  public void displayAbort(String error) {
    if (!Constants.JPAKE_ERROR_USERABORT.equals(error) && !hasInternet()) {
      setContentView(R.layout.sync_setup_nointernet);
      return;
    }
    if (pairWithPin) {
      // Clear PIN entries and display error.
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
    Logger.debug(LOG_TAG, "abort reason: " + error);
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

  @SuppressWarnings("unchecked")
  protected JSONObject makeAccountJSON(String username, String password,
                                       String syncKey, String serverURL) {

    JSONObject jAccount = new JSONObject();

    // Hack to try to keep Java 1.7 from complaining about unchecked types,
    // despite the presence of SuppressWarnings.
    HashMap<String, String> fields = (HashMap<String, String>) jAccount;

    fields.put(Constants.JSON_KEY_SYNCKEY,  syncKey);
    fields.put(Constants.JSON_KEY_ACCOUNT,  username);
    fields.put(Constants.JSON_KEY_PASSWORD, password);
    fields.put(Constants.JSON_KEY_SERVER,   serverURL);

    Logger.debug(LOG_TAG, "Extracted account data: " + jAccount.toJSONString());
    return jAccount;
  }

  /**
   * Device has finished key exchange, waiting for remote device to set up or
   * link to a Sync account. Display "waiting for other device" dialog.
   */
  public void onPaired() {
    // Extract Sync account data.
    Account[] accts = mAccountManager.getAccountsByType(Constants.ACCOUNTTYPE_SYNC);
    if (accts.length == 0) {
      // Error, no account present.
      Logger.error(LOG_TAG, "No accounts present.");
      displayAbort(Constants.JPAKE_ERROR_INVALID);
      return;
    }

    // TODO: Single account supported. Create account selection if spec changes.
    Account account = accts[0];
    String username  = account.name;
    String password  = mAccountManager.getPassword(account);
    String syncKey   = mAccountManager.getUserData(account, Constants.OPTION_SYNCKEY);
    String serverURL = mAccountManager.getUserData(account, Constants.OPTION_SERVER);

    JSONObject jAccount = makeAccountJSON(username, password, syncKey, serverURL);
    try {
      jClient.sendAndComplete(jAccount);
    } catch (JPakeNoActivePairingException e) {
      Logger.error(LOG_TAG, "No active J-PAKE pairing.", e);
      displayAbort(Constants.JPAKE_ERROR_INVALID);
    }
  }

  /**
   * J-PAKE pairing has started, but when this device has generated the PIN for
   * pairing, does not require UI feedback to user.
   */
  public void onPairingStart() {
    if (!pairWithPin) {
      runOnUiThread(new Runnable() {
        @Override
        public void run() {
          setContentView(R.layout.sync_setup_jpake_waiting);
        }
      });
      return;
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

      Logger.debug(LOG_TAG, "Using account manager " + mAccountManager);
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
    Logger.debug(LOG_TAG, "Checking internet connectivity.");
    ConnectivityManager connManager = (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
    NetworkInfo network = connManager.getActiveNetworkInfo();

    if (network != null && network.isConnected()) {
      Logger.debug(LOG_TAG, network + " is connected.");
      return true;
    }
    Logger.debug(LOG_TAG, "No connected networks.");
    return false;
  }

  /**
   * Displays layout for entering a PIN from another device.
   * A Sync Account has already been set up.
   */
  private void displayPairWithPin() {
    Logger.debug(LOG_TAG, "PairWithPin initiated.");
    runOnUiThread(new Runnable() {

      @Override
      public void run() {
        setContentView(R.layout.sync_setup_pair);
        connectButton = (Button) findViewById(R.id.pair_button_connect);
        pinError = (LinearLayout) findViewById(R.id.pair_error);

        row1 = (EditText) findViewById(R.id.pair_row1);
        row2 = (EditText) findViewById(R.id.pair_row2);
        row3 = (EditText) findViewById(R.id.pair_row3);

        row1.addTextChangedListener(new TextWatcher() {
          @Override
          public void afterTextChanged(Editable s) {
             activateButton(connectButton, pinEntryCompleted());
             if (s.length() == 4) {
               row2.requestFocus();
             }
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
            activateButton(connectButton, pinEntryCompleted());
            if (s.length() == 4) {
              row3.requestFocus();
            }
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
    });
  }

  /**
   * Displays layout with PIN for pairing with another device.
   * No Sync Account has been set up yet.
   */
  private void displayReceiveNoPin() {
    Logger.debug(LOG_TAG, "ReceiveNoPin initiated");
    runOnUiThread(new Runnable(){

      @Override
      public void run() {
        setContentView(R.layout.sync_setup);

        // Set up UI.
        setupTitleView = ((TextView) findViewById(R.id.setup_title));
        setupSubtitleView = (TextView) findViewById(R.id.setup_subtitle);
        setupNoDeviceLinkTitleView = (TextView) findViewById(R.id.link_nodevice);
        pinTextView1 = ((TextView) findViewById(R.id.text_pin1));
        pinTextView2 = ((TextView) findViewById(R.id.text_pin2));
        pinTextView3 = ((TextView) findViewById(R.id.text_pin3));

        // UI checks.
        if (setupTitleView == null) {
          Logger.error(LOG_TAG, "No title view.");
        }
        if (setupSubtitleView == null) {
          Logger.error(LOG_TAG, "No subtitle view.");
        }
        if (setupNoDeviceLinkTitleView == null) {
          Logger.error(LOG_TAG, "No 'no device' link view.");
        }
      }
    });
  }
}
