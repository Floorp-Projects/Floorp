/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.config.activities;

import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.ThreadPool;
import org.mozilla.gecko.sync.setup.SyncAccounts;
import org.mozilla.gecko.sync.syncadapter.SyncAdapter;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnDismissListener;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.widget.ListView;
import android.widget.Toast;

/**
 * Provides a user-facing interface for selecting engines to sync. This activity
 * can be launched from the Sync Settings preferences screen, and will save the
 * selected engines to the
 * <code>SyncConfiguration.PREF_USER_SELECTED_ENGINES_TO_SYNC</code> pref.
 *
 * On launch, it displays the engines stored in the saved pref if it exists, or
 * <code>SyncConfiguration.enabledEngineNames()</code> if it doesn't, defaulting
 * to <code>SyncConfiguration.validEngineNames()</code> if neither exists.
 *
 * During a sync, the
 * <code>SyncConfiguration.PREF_USER_SELECTED_ENGINES_TO_SYNC</code> pref will
 * be cleared after the engine changes are applied to meta/global.
 */

public class SelectEnginesActivity extends Activity implements
    DialogInterface.OnClickListener, DialogInterface.OnMultiChoiceClickListener {
  public final static String LOG_TAG = "SelectEnginesAct";

  protected AccountManager mAccountManager;
  protected Context mContext;

  // Collections names corresponding to displayed (localized) engine options.
  final String[] _collectionsNames = new String[] {
    "bookmarks",
    "passwords",
    "history",
    "tabs"
  };

  // Engine names localized for display in Sync Settings.
  protected String[] _options;
  // Selection state of engines corresponding to _options array.
  final boolean[] _selections = new boolean[_collectionsNames.length];
  protected Account account;
  protected SharedPreferences accountPrefs;

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    mContext = getApplicationContext();
    mAccountManager = AccountManager.get(mContext);
    _options = new String[] {
        getString(R.string.sync_configure_engines_title_bookmarks),
        getString(R.string.sync_configure_engines_title_passwords),
        getString(R.string.sync_configure_engines_title_history),
        getString(R.string.sync_configure_engines_title_tabs) };

    // Fetch account prefs for configuring engines.
    ThreadPool.run(new Runnable() {
      @Override
      public void run() {
        Account[] accounts = SyncAccounts.syncAccounts(mContext);
        if (accounts.length == 0) {
          Logger.error(LOG_TAG, "Failed to get account!");
          finish();
          return;
        } else {
          // Only supports one account per type.
          account = accounts[0];
          try {
            if (accountPrefs == null) {
              accountPrefs = SyncAccounts.blockingPrefsFromDefaultProfileV0(mContext, mAccountManager, account);
            }
          } catch (Exception e) {
            Logger.error(LOG_TAG, "Failed to get sync account info or shared preferences.", e);
            finish();
          }
          setSelectionsInArray(getEnginesToSelect(accountPrefs), _selections);
        }
      }
    });
  }

  @Override
  public void onResume() {
    super.onResume();
    if (accountPrefs != null) {
      setSelectionsInArray(getEnginesToSelect(accountPrefs), _selections);
    }
    AlertDialog dialog = new AlertDialog.Builder(this)
        .setTitle(R.string.sync_configure_engines_sync_my_title)
        .setMultiChoiceItems(_options, _selections, this)
        .setIcon(R.drawable.icon).setPositiveButton(android.R.string.ok, this)
        .setNegativeButton(android.R.string.cancel, this).create();

    dialog.setOnDismissListener(new OnDismissListener() {
      @Override
      public void onDismiss(DialogInterface dialog) {
        finish();
      }
    });

    dialog.show();
  }

  private static Set<String> getEnginesFromPrefs(SharedPreferences syncPrefs) {
    Set<String> engines = SyncConfiguration.getEnabledEngineNames(syncPrefs);
    if (engines == null) {
      engines = SyncConfiguration.validEngineNames();
    }
    return engines;
  }

  /**
   * Fetches the engine names that should be displayed as selected for syncing.
   * Check first for selected engines set by this activity, then the enabled
   * engines, and finally default to the set of valid engine names for Android
   * Sync if neither exists.
   *
   * @param syncPrefs
   *          <code>SharedPreferences</code> of Account being modified.
   * @return Set<String> of engine names to display as selected. Should never be
   *         null.
   */
  public static Set<String> getEnginesToSelect(SharedPreferences syncPrefs) {
    Set<String> engines = getEnginesFromPrefs(syncPrefs);
    Map<String, Boolean> engineSelections = SyncConfiguration.getUserSelectedEngines(syncPrefs);
    if (engineSelections != null) {
      for (Entry<String, Boolean> pair : engineSelections.entrySet()) {
        if (pair.getValue()) {
          engines.add(pair.getKey());
        } else {
          engines.remove(pair.getKey());
        }
      }
    }
    return engines;
  }

  public void setSelectionsInArray(Set<String> selected, boolean[] array) {
    for (int i = 0; i < _collectionsNames.length; i++) {
      array[i] = selected.contains(_collectionsNames[i]);
    }
  }

  @Override
  public void onClick(DialogInterface dialog, int which) {
    if (which == DialogInterface.BUTTON_POSITIVE) {
      Logger.debug(LOG_TAG, "Saving selected engines.");
      saveSelections();
      setResult(RESULT_OK);
      Toast.makeText(this, R.string.sync_notification_savedprefs, Toast.LENGTH_SHORT).show();
    } else {
      setResult(RESULT_CANCELED);
    }
    finish();
  }

  @Override
  public void onClick(DialogInterface dialog, int which, boolean isChecked) {
    // Display multi-selection clicks in UI.
    _selections[which] = isChecked;
    ListView selectionsList = ((AlertDialog) dialog).getListView();
    selectionsList.setItemChecked(which, isChecked);
  }

  /**
   * Persists engine selection state to SharedPreferences if it has changed.
   *
   * @return true if changed, false otherwise.
   */
  private void saveSelections() {
    boolean[] origSelections = new boolean[_options.length];
    setSelectionsInArray(getEnginesFromPrefs(accountPrefs), origSelections);

    Map<String, Boolean> engineSelections = new HashMap<String, Boolean>();
    for (int i = 0; i < _selections.length; i++) {
      if (_selections[i] != origSelections[i]) {
        engineSelections.put(_collectionsNames[i], _selections[i]);
      }
    }

    // No GlobalSession.config, so store directly to prefs.
    SyncConfiguration.storeSelectedEnginesToPrefs(accountPrefs, engineSelections);

    // Request immediate sync.
    SyncAdapter.requestImmediateSync(account, null);
  }
}
