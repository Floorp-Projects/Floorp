/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import java.util.HashMap;
import java.util.Map;
import java.util.Set;

import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.Married;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.setup.activities.ActivityUtils;

import android.content.ContentResolver;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.ViewFlipper;

/**
 * Activity which displays account status.
 */
public class FxAccountStatusActivity extends FxAccountAbstractActivity implements OnClickListener {
  private static final String LOG_TAG = FxAccountStatusActivity.class.getSimpleName();

  // When a checkbox is toggled, wait 5 seconds (for other checkbox actions)
  // before trying to sync. Should we quit the activity before the sync request
  // happens, that's okay: the runnable will run if the UI thread is still
  // around to service it, and since we're not updating any UI, we'll just
  // schedule the sync as usual. See also comment below about garbage
  // collection.
  private static final long DELAY_IN_MILLISECONDS_BEFORE_REQUESTING_SYNC = 5 * 1000;

  // Set in onCreate.
  protected TextView syncStatusTextView;
  protected ViewFlipper connectionStatusViewFlipper;
  protected View connectionStatusUnverifiedView;
  protected View connectionStatusSignInView;
  protected TextView emailTextView;

  protected CheckBox bookmarksCheckBox;
  protected CheckBox historyCheckBox;
  protected CheckBox passwordsCheckBox;
  protected CheckBox tabsCheckBox;

  // Used to post delayed sync requests.
  protected Handler handler;

  // Set in onResume.
  protected AndroidFxAccount fxAccount;
  // Member variable so that re-posting pushes back the already posted instance.
  // This Runnable references the fxAccount above, so it is not specific a
  // single account. (That is, it does not capture a single account instance.)
  protected Runnable requestSyncRunnable;

  public FxAccountStatusActivity() {
    super(CANNOT_RESUME_WHEN_NO_ACCOUNTS_EXIST);
  }

  /**
   * {@inheritDoc}
   */
  @Override
  public void onCreate(Bundle icicle) {
    Logger.setThreadLogTag(FxAccountConstants.GLOBAL_LOG_TAG);
    Logger.debug(LOG_TAG, "onCreate(" + icicle + ")");

    super.onCreate(icicle);
    setContentView(R.layout.fxaccount_status);

    syncStatusTextView = (TextView) ensureFindViewById(null, R.id.sync_status_text, "sync status text");
    connectionStatusViewFlipper = (ViewFlipper) ensureFindViewById(null, R.id.connection_status_view, "connection status frame layout");
    connectionStatusUnverifiedView = ensureFindViewById(null, R.id.unverified_view, "unverified view");
    connectionStatusSignInView = ensureFindViewById(null, R.id.sign_in_view, "sign in view");

    launchActivityOnClick(connectionStatusSignInView, FxAccountUpdateCredentialsActivity.class);

    connectionStatusUnverifiedView.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        FxAccountConfirmAccountActivity.resendCode(getApplicationContext(), fxAccount);
        launchActivity(FxAccountConfirmAccountActivity.class);
      }
    });

    emailTextView = (TextView) findViewById(R.id.email);

    bookmarksCheckBox = (CheckBox) findViewById(R.id.bookmarks_checkbox);
    historyCheckBox = (CheckBox) findViewById(R.id.history_checkbox);
    passwordsCheckBox = (CheckBox) findViewById(R.id.passwords_checkbox);
    tabsCheckBox = (CheckBox) findViewById(R.id.tabs_checkbox);
    bookmarksCheckBox.setOnClickListener(this);
    historyCheckBox.setOnClickListener(this);
    passwordsCheckBox.setOnClickListener(this);
    tabsCheckBox.setOnClickListener(this);

    handler = new Handler(); // Attached to current (UI) thread.
    // Runnable is not specific to one Firefox Account. This runnable will keep
    // a reference to this activity alive, but we expect posted runnables to be
    // serviced very quickly, so this is not an issue.
    requestSyncRunnable = new Runnable() {
      @Override
      public void run() {
        if (fxAccount == null) {
          return;
        }
        Logger.info(LOG_TAG, "Requesting a sync sometime soon.");
        // Request a sync, but not necessarily an immediate sync.
        ContentResolver.requestSync(fxAccount.getAndroidAccount(), BrowserContract.AUTHORITY, Bundle.EMPTY);
        // SyncAdapter.requestImmediateSync(fxAccount.getAndroidAccount(), null);
      }
    };

    TextView privacyView = (TextView) findViewById(R.id.fxaccount_status_linkprivacy);
    ActivityUtils.linkTextView(privacyView, R.string.fxaccount_policy_linkprivacy, R.string.fxaccount_link_pn);

    TextView tosView = (TextView) findViewById(R.id.fxaccount_status_linktos);
    ActivityUtils.linkTextView(tosView, R.string.fxaccount_policy_linktos, R.string.fxaccount_link_tos);

    if (FxAccountConstants.LOG_PERSONAL_INFORMATION) {
      createDebugButtons();
    }
  }

  @Override
  public void onResume() {
    super.onResume();
    this.fxAccount = getAndroidFxAccount();
    if (fxAccount == null) {
      Logger.warn(LOG_TAG, "Could not get Firefox Account.");
      setResult(RESULT_CANCELED);
      finish();
      return;
    }

    refresh();
  }

  protected void setCheckboxesEnabled(boolean enabled) {
    bookmarksCheckBox.setEnabled(enabled);
    historyCheckBox.setEnabled(enabled);
    passwordsCheckBox.setEnabled(enabled);
    tabsCheckBox.setEnabled(enabled);
  }

  protected void showNeedsUpgrade() {
    syncStatusTextView.setText(R.string.fxaccount_status_sync);
    connectionStatusViewFlipper.setVisibility(View.VISIBLE);
    connectionStatusViewFlipper.setDisplayedChild(0);
    setCheckboxesEnabled(false);
  }

  protected void showNeedsPassword() {
    syncStatusTextView.setText(R.string.fxaccount_status_sync);
    connectionStatusViewFlipper.setVisibility(View.VISIBLE);
    connectionStatusViewFlipper.setDisplayedChild(1);
    setCheckboxesEnabled(false);
  }

  protected void showNeedsVerification() {
    syncStatusTextView.setText(R.string.fxaccount_status_sync);
    connectionStatusViewFlipper.setVisibility(View.VISIBLE);
    connectionStatusViewFlipper.setDisplayedChild(2);
    setCheckboxesEnabled(false);
  }

  protected void showConnected() {
    syncStatusTextView.setText(R.string.fxaccount_status_sync_enabled);
    connectionStatusViewFlipper.setVisibility(View.GONE);
    setCheckboxesEnabled(true);
  }

  protected void refresh() {
    emailTextView.setText(fxAccount.getEmail());

    // Interrogate the Firefox Account's state.
    State state = fxAccount.getState();
    switch (state.getNeededAction()) {
    case NeedsUpgrade:
      showNeedsUpgrade();
      break;
    case NeedsPassword:
      showNeedsPassword();
      break;
    case NeedsVerification:
      showNeedsVerification();
      break;
    default:
      showConnected();
    }

    updateSelectedEngines();
  }

  protected void updateSelectedEngines() {
    try {
      SharedPreferences syncPrefs = fxAccount.getSyncPrefs();
      Map<String, Boolean> engines = SyncConfiguration.getUserSelectedEngines(syncPrefs);
      if (engines != null) {
        bookmarksCheckBox.setChecked(engines.containsKey("bookmarks") && engines.get("bookmarks"));
        historyCheckBox.setChecked(engines.containsKey("history") && engines.get("history"));
        passwordsCheckBox.setChecked(engines.containsKey("passwords") && engines.get("passwords"));
        tabsCheckBox.setChecked(engines.containsKey("tabs") && engines.get("tabs"));
        return;
      }

      // We don't have user specified preferences.  Perhaps we have seen a meta/global?
      Set<String> enabledNames = SyncConfiguration.getEnabledEngineNames(syncPrefs);
      if (enabledNames != null) {
        bookmarksCheckBox.setChecked(enabledNames.contains("bookmarks"));
        historyCheckBox.setChecked(enabledNames.contains("history"));
        passwordsCheckBox.setChecked(enabledNames.contains("passwords"));
        tabsCheckBox.setChecked(enabledNames.contains("tabs"));
        return;
      }

      // Okay, we don't have userSelectedEngines or enabledEngines. That means
      // the user hasn't specified to begin with, we haven't specified here, and
      // we haven't already seen, Sync engines. We don't know our state, so
      // let's check everything (the default) and disable everything.
      bookmarksCheckBox.setChecked(true);
      historyCheckBox.setChecked(true);
      passwordsCheckBox.setChecked(true);
      tabsCheckBox.setChecked(true);
      setCheckboxesEnabled(false);
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Got exception getting engines to select; ignoring.", e);
      return;
    }
  }

  @Override
  public void onClick(View view) {
    if (view == bookmarksCheckBox ||
        view == historyCheckBox ||
        view == passwordsCheckBox ||
        view == tabsCheckBox) {
      saveEngineSelections();
    }
  }

  protected void saveEngineSelections() {
    Map<String, Boolean> engineSelections = new HashMap<String, Boolean>();
    engineSelections.put("bookmarks", bookmarksCheckBox.isChecked());
    engineSelections.put("history", historyCheckBox.isChecked());
    engineSelections.put("passwords", passwordsCheckBox.isChecked());
    engineSelections.put("tabs", tabsCheckBox.isChecked());
    Logger.info(LOG_TAG, "Persisting engine selections: " + engineSelections.toString());

    try {
      // No GlobalSession.config, so store directly to prefs. We'd like to do
      // this on a background thread to avoid IO on the main thread and strict
      // mode warnings, but all in good time.
      SyncConfiguration.storeSelectedEnginesToPrefs(fxAccount.getSyncPrefs(), engineSelections);
      requestDelayedSync();
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Got exception persisting selected engines; ignoring.", e);
      return;
    }
  }

  protected void requestDelayedSync() {
    Logger.info(LOG_TAG, "Posting a delayed request for a sync sometime soon.");
    handler.removeCallbacks(requestSyncRunnable);
    handler.postDelayed(requestSyncRunnable, DELAY_IN_MILLISECONDS_BEFORE_REQUESTING_SYNC);
  }

  protected void createDebugButtons() {
    if (!FxAccountConstants.LOG_PERSONAL_INFORMATION) {
      return;
    }

    final LinearLayout existingUserView = (LinearLayout) findViewById(R.id.existing_user);
    if (existingUserView == null) {
      return;
    }

    final LinearLayout debugButtonsView = new LinearLayout(this);
    debugButtonsView.setOrientation(LinearLayout.VERTICAL);
    debugButtonsView.setLayoutParams(new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));
    existingUserView.addView(debugButtonsView, existingUserView.getChildCount());

    Button button;

    button = new Button(this);
    debugButtonsView.addView(button, debugButtonsView.getChildCount());
    button.setText("Refresh status view");
    button.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        Logger.info(LOG_TAG, "Refreshing.");
        refresh();
      }
    });

    button = new Button(this);
    debugButtonsView.addView(button, debugButtonsView.getChildCount());
    button.setText("Dump account details");
    button.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        fxAccount.dump();
      }
    });

    button = new Button(this);
    debugButtonsView.addView(button, debugButtonsView.getChildCount());
    button.setText("Force sync");
    button.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        Logger.info(LOG_TAG, "Syncing.");
        final Bundle extras = new Bundle();
        extras.putBoolean(ContentResolver.SYNC_EXTRAS_MANUAL, true);
        ContentResolver.requestSync(fxAccount.getAndroidAccount(), BrowserContract.AUTHORITY, extras);
        // No sense refreshing, since the sync will complete in the future.
      }
    });

    button = new Button(this);
    debugButtonsView.addView(button, debugButtonsView.getChildCount());
    button.setText("Forget certificate (if applicable)");
    button.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        State state = fxAccount.getState();
        try {
          Married married = (Married) state;
          Logger.info(LOG_TAG, "Moving to Cohabiting state: Forgetting certificate.");
          fxAccount.setState(married.makeCohabitingState());
          refresh();
        } catch (ClassCastException e) {
          Logger.info(LOG_TAG, "Not in Married state; can't forget certificate.");
          // Ignore.
        }
      }
    });

    button = new Button(this);
    debugButtonsView.addView(button, debugButtonsView.getChildCount());
    button.setText("Require password");
    button.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        Logger.info(LOG_TAG, "Moving to Separated state: Forgetting password.");
        State state = fxAccount.getState();
        fxAccount.setState(state.makeSeparatedState());
        refresh();
      }
    });

    button = new Button(this);
    debugButtonsView.addView(button, debugButtonsView.getChildCount());
    button.setText("Require upgrade");
    button.setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        Logger.info(LOG_TAG, "Moving to Doghouse state: Requiring upgrade.");
        State state = fxAccount.getState();
        fxAccount.setState(state.makeDoghouseState());
        refresh();
      }
    });
  }
}
