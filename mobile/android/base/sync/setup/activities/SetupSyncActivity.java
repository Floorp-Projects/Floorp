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
import org.mozilla.gecko.sync.setup.Constants;

import android.accounts.Account;
import android.accounts.AccountAuthenticatorActivity;
import android.accounts.AccountManager;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.TextView;

public class SetupSyncActivity extends AccountAuthenticatorActivity {
  private final static String LOG_TAG = "SetupSync";
  private TextView            setupTitleView;
  private TextView            setupNoDeviceLinkTitleView;
  private TextView            setupSubtitleView;
  private TextView            pinTextView;
  private JPakeClient         jClient;
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

    // Set Activity variables.
    mAccountManager = AccountManager.get(getApplicationContext());
    mContext = getApplicationContext();
  }

  @Override
  public void onResume() {
    Log.i(LOG_TAG, "Called SetupSyncActivity.onResume.");
    super.onResume();

    // Check whether Sync accounts exist; if so, display Pair text.
    AccountManager mAccountManager = AccountManager.get(this);
    Account[] accts = mAccountManager
        .getAccountsByType(Constants.ACCOUNTTYPE_SYNC);
    if (accts.length > 0) {
      authSuccess(false);
      // TODO: Change when:
      // 1. enable setting up multiple accounts.
      // 2. enable pair with PIN (entering pin, rather than displaying)
    } else {
      // Start J-PAKE for pairing if no accounts present.
      jClient = new JPakeClient(this);
      jClient.receiveNoPin();
    }
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

  // Controller methods
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
    // Start new JPakeClient for restarting J-PAKE.
    jClient = new JPakeClient(this);

    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        // Restart pairing process.
        jClient.receiveNoPin();
      }
    });
  }

  /**
   * Device has finished key exchange, waiting for remote device to set up or
   * link to a Sync account. Display "waiting for other device" dialog.
   */
  public void onPaired() {
    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        Intent intent = new Intent(mContext, SetupWaitingActivity.class);
        // TODO: respond with abort if canceled.
        startActivityForResult(intent, 0);
      }
    });
  }

  protected void onActivityResult(int requestCode, int resultCode, Intent data) {
    if (resultCode == RESULT_CANCELED) {
      displayAbort(Constants.JPAKE_ERROR_USERABORT);
    }
  }

  /**
   * On J-PAKE completion, store the Sync Account credentials sent by other
   * device. Display progress to user.
   *
   * @param jCreds
   */
  public void onComplete(JSONObject jCreds) {
    String accountName = (String) jCreds.get(Constants.JSON_KEY_ACCOUNT);
    String password    = (String) jCreds.get(Constants.JSON_KEY_PASSWORD);
    String syncKey     = (String) jCreds.get(Constants.JSON_KEY_SYNCKEY);
    String serverURL   = (String) jCreds.get(Constants.JSON_KEY_SERVER);

    final Intent intent = AccountActivity.createAccount(mAccountManager, accountName, syncKey, password, serverURL);
    setAccountAuthenticatorResult(intent.getExtras());

    setResult(RESULT_OK, intent);

    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        authSuccess(true);
      }
    });
  }

  private void authSuccess(boolean isSetup) {
    Intent intent = new Intent(mContext, SetupSuccessActivity.class);
    intent.putExtra(Constants.INTENT_EXTRA_IS_SETUP, isSetup);
    startActivity(intent);
    finish();
  }

  /**
   * J-PAKE pairing has started, but when this device has generated the PIN for
   * pairing, does not require UI feedback to user.
   */
  public void onPairingStart() {
    // Do nothing.
    // TODO: add in functionality if/when adding pairWithPIN.
  }
}
