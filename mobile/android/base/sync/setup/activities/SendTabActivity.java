/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup.activities;

import java.util.List;

import org.mozilla.gecko.R;
import org.mozilla.gecko.sync.CommandProcessor;
import org.mozilla.gecko.sync.CommandRunner;
import org.mozilla.gecko.sync.GlobalConstants;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.android.ClientsDatabaseAccessor;
import org.mozilla.gecko.sync.repositories.domain.ClientRecord;
import org.mozilla.gecko.sync.setup.Constants;
import org.mozilla.gecko.sync.stage.SyncClientsEngineStage;
import org.mozilla.gecko.sync.syncadapter.SyncAdapter;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.AsyncTask;
import android.os.Bundle;
import android.view.View;
import android.widget.ListView;
import android.widget.Toast;

public class SendTabActivity extends Activity {
  public static final String LOG_TAG = "SendTabActivity";
  private ClientRecordArrayAdapter arrayAdapter;
  private AccountManager accountManager;
  private Account localAccount;

  @Override
  public void onCreate(Bundle savedInstanceState) {
    setTheme(R.style.SyncTheme);
    super.onCreate(savedInstanceState);
  }

  @Override
  public void onResume() {
    Logger.info(LOG_TAG, "Called SendTabActivity.onResume.");
    super.onResume();

    redirectIfNoSyncAccount();
    registerDisplayURICommand();

    setContentView(R.layout.sync_send_tab);
    final ListView listview = (ListView) findViewById(R.id.device_list);
    listview.setItemsCanFocus(true);
    listview.setTextFilterEnabled(true);
    listview.setChoiceMode(ListView.CHOICE_MODE_MULTIPLE);
    enableSend(false);

    // Fetching the client list hits the clients database, so we spin this onto
    // a background task.
    final Context context = this;
    new AsyncTask<Void, Void, ClientRecord[]>() {

      @Override
      protected ClientRecord[] doInBackground(Void... params) {
        return getClientArray();
      }

      @Override
      protected void onPostExecute(final ClientRecord[] clientArray) {
        // We're allowed to update the UI from here.
        arrayAdapter = new ClientRecordArrayAdapter(context, R.layout.sync_list_item, clientArray);
        listview.setAdapter(arrayAdapter);
      }
    }.execute();
  }

  private static void registerDisplayURICommand() {
    final CommandProcessor processor = CommandProcessor.getProcessor();
    processor.registerCommand("displayURI", new CommandRunner(3) {
      @Override
      public void executeCommand(final GlobalSession session, List<String> args) {
        CommandProcessor.displayURI(args, session.getContext());
      }
    });
  }

  private void redirectIfNoSyncAccount() {
    accountManager = AccountManager.get(getApplicationContext());
    Account[] accts = accountManager.getAccountsByType(GlobalConstants.ACCOUNTTYPE_SYNC);

    // A Sync account exists.
    if (accts.length > 0) {
      localAccount = accts[0];
      return;
    }

    Intent intent = new Intent(this, RedirectToSetupActivity.class);
    intent.setFlags(Constants.FLAG_ACTIVITY_REORDER_TO_FRONT_NO_ANIMATION);
    startActivity(intent);
    finish();
  }

  /**
   * @return Return null if there is no account set up. Return the account GUID otherwise.
   */
  private String getAccountGUID() {
    if (accountManager == null || localAccount == null) {
      return null;
    }
    return accountManager.getUserData(localAccount, Constants.ACCOUNT_GUID);
  }

  public void sendClickHandler(View view) {
    Logger.info(LOG_TAG, "Send was clicked.");
    Bundle extras = this.getIntent().getExtras();
    final String uri = extras.getString(Intent.EXTRA_TEXT);
    final String title = extras.getString(Intent.EXTRA_SUBJECT);
    final CommandProcessor processor = CommandProcessor.getProcessor();

    final String clientGUID = getAccountGUID();
    final List<String> guids = arrayAdapter.getCheckedGUIDs();

    if (clientGUID == null || guids == null) {
      // Should never happen.
      Logger.warn(LOG_TAG, "clientGUID? " + (clientGUID == null) + " or guids? " + (guids == null) +
          " was null; aborting without sending tab.");
      finish();
      return;
    }

    // Perform tab sending on another thread.
    new Thread() {
      @Override
      public void run() {
        for (String guid : guids) {
          processor.sendURIToClientForDisplay(uri, guid, title, clientGUID, getApplicationContext());
        }

        Logger.info(LOG_TAG, "Requesting immediate clients stage sync.");
        SyncAdapter.requestImmediateSync(localAccount, new String[] { SyncClientsEngineStage.COLLECTION_NAME });
      }
    }.start();

    notifyAndFinish();
  }

  /**
   * Notify the user that tabs were sent and then finish the activity.
   * <p>
   * This is a bit of a misnomer: we wrote "displayURI" commands to the local
   * command database, and they will be sent on next sync. There is no way to
   * verify that the commands were successfully received by the intended remote
   * client, so we lie and say they were sent.
   */
  private void notifyAndFinish() {
    Toast.makeText(this, R.string.sync_text_tab_sent, Toast.LENGTH_LONG).show();
    finish();
  }

  public void enableSend(boolean shouldEnable) {
    View sendButton = findViewById(R.id.send_button);
    sendButton.setEnabled(shouldEnable);
    sendButton.setClickable(shouldEnable);
  }

  protected ClientRecord[] getClientArray() {
    ClientsDatabaseAccessor db = new ClientsDatabaseAccessor(this.getApplicationContext());

    try {
      return db.fetchAllClients().values().toArray(new ClientRecord[0]);
    } catch (NullCursorException e) {
      Logger.warn(LOG_TAG, "NullCursorException while populating device list.", e);
      return null;
    } finally {
      db.close();
    }
  }
}
