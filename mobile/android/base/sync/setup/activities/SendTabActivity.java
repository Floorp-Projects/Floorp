package org.mozilla.gecko.sync.setup.activities;

import java.util.List;

import org.mozilla.gecko.R;
import org.mozilla.gecko.sync.CommandProcessor;
import org.mozilla.gecko.sync.CommandRunner;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.android.ClientsDatabaseAccessor;
import org.mozilla.gecko.sync.repositories.domain.ClientRecord;
import org.mozilla.gecko.sync.setup.Constants;
import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;
import android.app.Dialog;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.Window;
import android.widget.Button;
import android.widget.ListView;

public class SendTabActivity extends Activity {
  public static final String LOG_TAG = "SendTabActivity";
  private ListView listview;
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
    arrayAdapter = new ClientRecordArrayAdapter(this, R.layout.sync_list_item, getClientArray());

    listview = (ListView) findViewById(R.id.device_list);
    listview.setAdapter(arrayAdapter);
    listview.setItemsCanFocus(true);
    listview.setTextFilterEnabled(true);
    listview.setChoiceMode(ListView.CHOICE_MODE_MULTIPLE);
    enableSend(false);
  }

  private void registerDisplayURICommand() {
    final CommandProcessor processor = CommandProcessor.getProcessor();
    processor.registerCommand("displayURI", new CommandRunner(3) {
      @Override
      public void executeCommand(List<String> args) {
        CommandProcessor.displayURI(args, getApplicationContext());
      }
    });
  }

  private void redirectIfNoSyncAccount() {
    accountManager = AccountManager.get(getApplicationContext());
    Account[] accts = accountManager.getAccountsByType(Constants.ACCOUNTTYPE_SYNC);

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

    final String[] guids = arrayAdapter.getCheckedGUIDs();

    // Perform tab sending on another thread.
    new Thread() {
      @Override
      public void run() {
        for (int i = 0; i < guids.length; i++) {
          processor.sendURIToClientForDisplay(uri, guids[i], title, getAccountGUID(), getApplicationContext());
        }
      }
    }.start();

    openDialog();
  }

  private void openDialog() {
    final Dialog dialog = new Dialog(this);
    dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
    dialog.setContentView(R.layout.sync_custom_popup);
    Button button = (Button) dialog.findViewById(R.id.continue_browsing);
    button.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        dialog.dismiss();
        finish();
      }
    });

    dialog.show();
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
