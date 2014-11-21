/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import java.util.HashMap;
import java.util.Map;
import java.util.Set;

import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.preferences.PreferenceFragment;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.Married;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.sync.FxAccountSyncStatusHelper;
import org.mozilla.gecko.fxa.tasks.FxAccountCodeResender;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.SharedPreferencesClientsDataDelegate;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.util.HardwareUtils;

import android.accounts.Account;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Handler;
import android.preference.CheckBoxPreference;
import android.preference.EditTextPreference;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceCategory;
import android.preference.PreferenceScreen;
import android.text.TextUtils;
import android.text.format.DateUtils;

/**
 * A fragment that displays the status of an AndroidFxAccount.
 * <p>
 * The owning activity is responsible for providing an AndroidFxAccount at
 * appropriate times.
 */
public class FxAccountStatusFragment
    extends PreferenceFragment
    implements OnPreferenceClickListener, OnPreferenceChangeListener {
  private static final String LOG_TAG = FxAccountStatusFragment.class.getSimpleName();

  // When a checkbox is toggled, wait 5 seconds (for other checkbox actions)
  // before trying to sync. Should we kill off the fragment before the sync
  // request happens, that's okay: the runnable will run if the UI thread is
  // still around to service it, and since we're not updating any UI, we'll just
  // schedule the sync as usual. See also comment below about garbage
  // collection.
  private static final long DELAY_IN_MILLISECONDS_BEFORE_REQUESTING_SYNC = 5 * 1000;

  // By default, the auth/account server preference is only shown when the
  // account is configured to use a custom server. In debug mode, this is set.
  private static boolean ALWAYS_SHOW_AUTH_SERVER = false;

  // By default, the Sync server preference is only shown when the account is
  // configured to use a custom Sync server. In debug mode, this is set.
  private static boolean ALWAYS_SHOW_SYNC_SERVER = false;

  protected PreferenceCategory accountCategory;
  protected Preference emailPreference;
  protected Preference authServerPreference;

  protected Preference needsPasswordPreference;
  protected Preference needsUpgradePreference;
  protected Preference needsVerificationPreference;
  protected Preference needsMasterSyncAutomaticallyEnabledPreference;
  protected Preference needsAccountEnabledPreference;

  protected PreferenceCategory syncCategory;

  protected CheckBoxPreference bookmarksPreference;
  protected CheckBoxPreference historyPreference;
  protected CheckBoxPreference tabsPreference;
  protected CheckBoxPreference passwordsPreference;

  protected EditTextPreference deviceNamePreference;
  protected Preference syncServerPreference;
  protected Preference morePreference;
  protected Preference syncNowPreference;

  protected volatile AndroidFxAccount fxAccount;
  // The contract is: when fxAccount is non-null, then clientsDataDelegate is
  // non-null.  If violated then an IllegalStateException is thrown.
  protected volatile SharedPreferencesClientsDataDelegate clientsDataDelegate;

  // Used to post delayed sync requests.
  protected Handler handler;

  // Member variable so that re-posting pushes back the already posted instance.
  // This Runnable references the fxAccount above, but it is not specific to a
  // single account. (That is, it does not capture a single account instance.)
  protected Runnable requestSyncRunnable;

  protected final InnerSyncStatusDelegate syncStatusDelegate = new InnerSyncStatusDelegate();

  protected Preference ensureFindPreference(String key) {
    Preference preference = findPreference(key);
    if (preference == null) {
      throw new IllegalStateException("Could not find preference with key: " + key);
    }
    return preference;
  }

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    // We need to do this before we can query the hardware menu button state.
    // We're guaranteed to have an activity at this point (onAttach is called
    // before onCreate). It's okay to call this multiple times (with different
    // contexts).
    HardwareUtils.init(getActivity());

    addPreferences();
  }

  protected void addPreferences() {
    addPreferencesFromResource(R.xml.fxaccount_status_prefscreen);

    accountCategory = (PreferenceCategory) ensureFindPreference("signed_in_as_category");
    emailPreference = ensureFindPreference("email");
    authServerPreference = ensureFindPreference("auth_server");

    needsPasswordPreference = ensureFindPreference("needs_credentials");
    needsUpgradePreference = ensureFindPreference("needs_upgrade");
    needsVerificationPreference = ensureFindPreference("needs_verification");
    needsMasterSyncAutomaticallyEnabledPreference = ensureFindPreference("needs_master_sync_automatically_enabled");
    needsAccountEnabledPreference = ensureFindPreference("needs_account_enabled");

    syncCategory = (PreferenceCategory) ensureFindPreference("sync_category");

    bookmarksPreference = (CheckBoxPreference) ensureFindPreference("bookmarks");
    historyPreference = (CheckBoxPreference) ensureFindPreference("history");
    tabsPreference = (CheckBoxPreference) ensureFindPreference("tabs");
    passwordsPreference = (CheckBoxPreference) ensureFindPreference("passwords");

    if (!FxAccountConstants.LOG_PERSONAL_INFORMATION) {
      removeDebugButtons();
    } else {
      connectDebugButtons();
      ALWAYS_SHOW_AUTH_SERVER = true;
      ALWAYS_SHOW_SYNC_SERVER = true;
    }

    needsPasswordPreference.setOnPreferenceClickListener(this);
    needsVerificationPreference.setOnPreferenceClickListener(this);
    needsAccountEnabledPreference.setOnPreferenceClickListener(this);

    bookmarksPreference.setOnPreferenceClickListener(this);
    historyPreference.setOnPreferenceClickListener(this);
    tabsPreference.setOnPreferenceClickListener(this);
    passwordsPreference.setOnPreferenceClickListener(this);

    deviceNamePreference = (EditTextPreference) ensureFindPreference("device_name");
    deviceNamePreference.setOnPreferenceChangeListener(this);

    syncServerPreference = ensureFindPreference("sync_server");
    morePreference = ensureFindPreference("more");
    morePreference.setOnPreferenceClickListener(this);

    syncNowPreference = ensureFindPreference("sync_now");
    syncNowPreference.setEnabled(true);
    syncNowPreference.setOnPreferenceClickListener(this);

    if (HardwareUtils.hasMenuButton()) {
      syncCategory.removePreference(morePreference);
    }
  }

  /**
   * We intentionally don't refresh here. Our owning activity is responsible for
   * providing an AndroidFxAccount to our refresh method in its onResume method.
   */
  @Override
  public void onResume() {
    super.onResume();
  }

  @Override
  public boolean onPreferenceClick(Preference preference) {
    if (preference == needsPasswordPreference) {
      Intent intent = new Intent(getActivity(), FxAccountUpdateCredentialsActivity.class);
      final Bundle extras = getExtrasForAccount();
      if (extras != null) {
        intent.putExtras(extras);
      }
      // Per http://stackoverflow.com/a/8992365, this triggers a known bug with
      // the soft keyboard not being shown for the started activity. Why, Android, why?
      intent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
      startActivity(intent);

      return true;
    }

    if (preference == needsVerificationPreference) {
      FxAccountCodeResender.resendCode(getActivity().getApplicationContext(), fxAccount);

      Intent intent = new Intent(getActivity(), FxAccountConfirmAccountActivity.class);
      // Per http://stackoverflow.com/a/8992365, this triggers a known bug with
      // the soft keyboard not being shown for the started activity. Why, Android, why?
      intent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
      startActivity(intent);

      return true;
    }

    if (preference == needsAccountEnabledPreference) {
      fxAccount.enableSyncing();
      refresh();

      return true;
    }

    if (preference == bookmarksPreference ||
        preference == historyPreference ||
        preference == passwordsPreference ||
        preference == tabsPreference) {
      saveEngineSelections();
      return true;
    }

    if (preference == morePreference) {
      getActivity().openOptionsMenu();
      return true;
    }

    if (preference == syncNowPreference) {
      if (fxAccount != null) {
        FirefoxAccounts.requestSync(fxAccount.getAndroidAccount(), FirefoxAccounts.FORCE, null, null);
      }
      return true;
    }

    return false;
  }

  protected Bundle getExtrasForAccount() {
    final Bundle extras = new Bundle();
    final ExtendedJSONObject o = new ExtendedJSONObject();
    o.put(FxAccountAbstractSetupActivity.JSON_KEY_AUTH, fxAccount.getAccountServerURI());
    final ExtendedJSONObject services = new ExtendedJSONObject();
    services.put(FxAccountAbstractSetupActivity.JSON_KEY_SYNC, fxAccount.getTokenServerURI());
    o.put(FxAccountAbstractSetupActivity.JSON_KEY_SERVICES, services);
    extras.putString(FxAccountAbstractSetupActivity.EXTRA_EXTRAS, o.toJSONString());
    return extras;
  }

  protected void setCheckboxesEnabled(boolean enabled) {
    bookmarksPreference.setEnabled(enabled);
    historyPreference.setEnabled(enabled);
    tabsPreference.setEnabled(enabled);
    passwordsPreference.setEnabled(enabled);
    // Since we can't sync, we can't update our remote client record.
    deviceNamePreference.setEnabled(enabled);
    syncNowPreference.setEnabled(enabled);
  }

  /**
   * Show at most one error preference, hiding all others.
   *
   * @param errorPreferenceToShow
   *          single error preference to show; if null, hide all error preferences
   */
  protected void showOnlyOneErrorPreference(Preference errorPreferenceToShow) {
    final Preference[] errorPreferences = new Preference[] {
        this.needsPasswordPreference,
        this.needsUpgradePreference,
        this.needsVerificationPreference,
        this.needsMasterSyncAutomaticallyEnabledPreference,
        this.needsAccountEnabledPreference,
    };
    for (Preference errorPreference : errorPreferences) {
      final boolean currentlyShown = null != findPreference(errorPreference.getKey());
      final boolean shouldBeShown = errorPreference == errorPreferenceToShow;
      if (currentlyShown == shouldBeShown) {
        continue;
      }
      if (shouldBeShown) {
        syncCategory.addPreference(errorPreference);
      } else {
        syncCategory.removePreference(errorPreference);
      }
    }
  }

  protected void showNeedsPassword() {
    syncCategory.setTitle(R.string.fxaccount_status_sync);
    showOnlyOneErrorPreference(needsPasswordPreference);
    setCheckboxesEnabled(false);
  }

  protected void showNeedsUpgrade() {
    syncCategory.setTitle(R.string.fxaccount_status_sync);
    showOnlyOneErrorPreference(needsUpgradePreference);
    setCheckboxesEnabled(false);
  }

  protected void showNeedsVerification() {
    syncCategory.setTitle(R.string.fxaccount_status_sync);
    showOnlyOneErrorPreference(needsVerificationPreference);
    setCheckboxesEnabled(false);
  }

  protected void showNeedsMasterSyncAutomaticallyEnabled() {
    syncCategory.setTitle(R.string.fxaccount_status_sync);
    showOnlyOneErrorPreference(needsMasterSyncAutomaticallyEnabledPreference);
    setCheckboxesEnabled(false);
  }

  protected void showNeedsAccountEnabled() {
    syncCategory.setTitle(R.string.fxaccount_status_sync);
    showOnlyOneErrorPreference(needsAccountEnabledPreference);
    setCheckboxesEnabled(false);
  }

  protected void showConnected() {
    syncCategory.setTitle(R.string.fxaccount_status_sync_enabled);
    showOnlyOneErrorPreference(null);
    setCheckboxesEnabled(true);
  }

  protected class InnerSyncStatusDelegate implements FirefoxAccounts.SyncStatusListener {
    protected final Runnable refreshRunnable = new Runnable() {
      @Override
      public void run() {
        refresh();
      }
    };

    @Override
    public Context getContext() {
      return FxAccountStatusFragment.this.getActivity();
    }

    @Override
    public Account getAccount() {
      return fxAccount.getAndroidAccount();
    }

    @Override
    public void onSyncStarted() {
      if (fxAccount == null) {
        return;
      }
      Logger.info(LOG_TAG, "Got sync started message; refreshing.");
      getActivity().runOnUiThread(refreshRunnable);
    }

    @Override
    public void onSyncFinished() {
      if (fxAccount == null) {
        return;
      }
      Logger.info(LOG_TAG, "Got sync finished message; refreshing.");
      getActivity().runOnUiThread(refreshRunnable);
    }
  }

  /**
   * Notify the fragment that a new AndroidFxAccount instance is current.
   * <p>
   * <b>Important:</b> call this method on the UI thread!
   * <p>
   * In future, this might be a Loader.
   *
   * @param fxAccount new instance.
   */
  public void refresh(AndroidFxAccount fxAccount) {
    if (fxAccount == null) {
      throw new IllegalArgumentException("fxAccount must not be null");
    }
    this.fxAccount = fxAccount;
    try {
      this.clientsDataDelegate = new SharedPreferencesClientsDataDelegate(fxAccount.getSyncPrefs(), getActivity().getApplicationContext());
    } catch (Exception e) {
      Logger.error(LOG_TAG, "Got exception fetching Sync prefs associated to Firefox Account; aborting.", e);
      // Something is terribly wrong; best to get a stack trace rather than
      // continue with a null clients delegate.
      throw new IllegalStateException(e);
    }

    handler = new Handler(); // Attached to current (assumed to be UI) thread.

    // Runnable is not specific to one Firefox Account. This runnable will keep
    // a reference to this fragment alive, but we expect posted runnables to be
    // serviced very quickly, so this is not an issue.
    requestSyncRunnable = new RequestSyncRunnable();

    // We would very much like register these status observers in bookended
    // onResume/onPause calls, but because the Fragment gets onResume during the
    // Activity's super.onResume, it hasn't yet been told its Firefox Account.
    // So we register the observer here (and remove it in onPause), and open
    // ourselves to the possibility that we don't have properly paired
    // register/unregister calls.
    FxAccountSyncStatusHelper.getInstance().startObserving(syncStatusDelegate);

    refresh();
  }

  @Override
  public void onPause() {
    super.onPause();
    FxAccountSyncStatusHelper.getInstance().stopObserving(syncStatusDelegate);
  }

  protected void hardRefresh() {
    // This is the only way to guarantee that the EditText dialogs created by
    // EditTextPreferences are re-created. This works around the issue described
    // at http://androiddev.orkitra.com/?p=112079.
    final PreferenceScreen statusScreen = (PreferenceScreen) ensureFindPreference("status_screen");
    statusScreen.removeAll();
    addPreferences();

    refresh();
  }

  protected void refresh() {
    // refresh is called from our onResume, which can happen before the owning
    // Activity tells us about an account (via our public
    // refresh(AndroidFxAccount) method).
    if (fxAccount == null) {
      throw new IllegalArgumentException("fxAccount must not be null");
    }

    emailPreference.setTitle(fxAccount.getEmail());
    updateAuthServerPreference();
    updateSyncServerPreference();

    try {
      // There are error states determined by Android, not the login state
      // machine, and we have a chance to present these states here.  We handle
      // them specially, since we can't surface these states as part of syncing,
      // because they generally stop syncs from happening regularly.

      // The action to enable syncing the Firefox Account doesn't require
      // leaving this activity, so let's present it first.
      final boolean isSyncing = fxAccount.isSyncing();
      if (!isSyncing) {
        showNeedsAccountEnabled();
        return;
      }

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

      // We check for the master setting last, since it is not strictly
      // necessary for the user to address this error state: it's really a
      // warning state. We surface it for the user's convenience, and to prevent
      // confused folks wondering why Sync is not working at all.
      final boolean masterSyncAutomatically = ContentResolver.getMasterSyncAutomatically();
      if (!masterSyncAutomatically) {
        showNeedsMasterSyncAutomaticallyEnabled();
        return;
      }
    } finally {
      // No matter our state, we should update the checkboxes.
      updateSelectedEngines();
    }

    final String clientName = clientsDataDelegate.getClientName();
    deviceNamePreference.setSummary(clientName);
    deviceNamePreference.setText(clientName);

    updateSyncNowPreference();
  }

  // This is a helper function similar to TabsAccessor.getLastSyncedString() to calculate relative "Last synced" time span.
  private String getLastSyncedString(final long startTime) {
    final CharSequence relativeTimeSpanString = DateUtils.getRelativeTimeSpanString(startTime);
    return getActivity().getResources().getString(R.string.fxaccount_status_last_synced, relativeTimeSpanString);
  }

  protected void updateSyncNowPreference() {
    final boolean currentlySyncing = fxAccount.isCurrentlySyncing();
    syncNowPreference.setEnabled(!currentlySyncing);
    if (currentlySyncing) {
      syncNowPreference.setTitle(R.string.fxaccount_status_syncing);
    } else {
      syncNowPreference.setTitle(R.string.fxaccount_status_sync_now);
    }
    final String lastSynced = getLastSyncedString(fxAccount.getLastSyncedTimestamp());
    syncNowPreference.setSummary(lastSynced);
  }

  protected void updateAuthServerPreference() {
    final String authServer = fxAccount.getAccountServerURI();
    final boolean shouldBeShown = ALWAYS_SHOW_AUTH_SERVER || !FxAccountConstants.DEFAULT_AUTH_SERVER_ENDPOINT.equals(authServer);
    final boolean currentlyShown = null != findPreference(authServerPreference.getKey());
    if (currentlyShown != shouldBeShown) {
      if (shouldBeShown) {
        accountCategory.addPreference(authServerPreference);
      } else {
        accountCategory.removePreference(authServerPreference);
      }
    }
    // Always set the summary, because on first run, the preference is visible,
    // and the above block will be skipped if there is a custom value.
    authServerPreference.setSummary(authServer);
  }

  protected void updateSyncServerPreference() {
    final String syncServer = fxAccount.getTokenServerURI();
    final boolean shouldBeShown = ALWAYS_SHOW_SYNC_SERVER || !FxAccountConstants.DEFAULT_TOKEN_SERVER_ENDPOINT.equals(syncServer);
    final boolean currentlyShown = null != findPreference(syncServerPreference.getKey());
    if (currentlyShown != shouldBeShown) {
      if (shouldBeShown) {
        syncCategory.addPreference(syncServerPreference);
      } else {
        syncCategory.removePreference(syncServerPreference);
      }
    }
    // Always set the summary, because on first run, the preference is visible,
    // and the above block will be skipped if there is a custom value.
    syncServerPreference.setSummary(syncServer);
  }

  /**
   * Query shared prefs for the current engine state, and update the UI
   * accordingly.
   * <p>
   * In future, we might want this to be on a background thread, or implemented
   * as a Loader.
   */
  protected void updateSelectedEngines() {
    try {
      SharedPreferences syncPrefs = fxAccount.getSyncPrefs();
      Map<String, Boolean> engines = SyncConfiguration.getUserSelectedEngines(syncPrefs);
      if (engines != null) {
        bookmarksPreference.setChecked(engines.containsKey("bookmarks") && engines.get("bookmarks"));
        historyPreference.setChecked(engines.containsKey("history") && engines.get("history"));
        passwordsPreference.setChecked(engines.containsKey("passwords") && engines.get("passwords"));
        tabsPreference.setChecked(engines.containsKey("tabs") && engines.get("tabs"));
        return;
      }

      // We don't have user specified preferences.  Perhaps we have seen a meta/global?
      Set<String> enabledNames = SyncConfiguration.getEnabledEngineNames(syncPrefs);
      if (enabledNames != null) {
        bookmarksPreference.setChecked(enabledNames.contains("bookmarks"));
        historyPreference.setChecked(enabledNames.contains("history"));
        passwordsPreference.setChecked(enabledNames.contains("passwords"));
        tabsPreference.setChecked(enabledNames.contains("tabs"));
        return;
      }

      // Okay, we don't have userSelectedEngines or enabledEngines. That means
      // the user hasn't specified to begin with, we haven't specified here, and
      // we haven't already seen, Sync engines. We don't know our state, so
      // let's check everything (the default) and disable everything.
      bookmarksPreference.setChecked(true);
      historyPreference.setChecked(true);
      passwordsPreference.setChecked(true);
      tabsPreference.setChecked(true);
      setCheckboxesEnabled(false);
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Got exception getting engines to select; ignoring.", e);
      return;
    }
  }

  /**
   * Persist engine selections to local shared preferences, and request a sync
   * to persist selections to remote storage.
   */
  protected void saveEngineSelections() {
    final Map<String, Boolean> engineSelections = new HashMap<String, Boolean>();
    engineSelections.put("bookmarks", bookmarksPreference.isChecked());
    engineSelections.put("history", historyPreference.isChecked());
    engineSelections.put("passwords", passwordsPreference.isChecked());
    engineSelections.put("tabs", tabsPreference.isChecked());

    // No GlobalSession.config, so store directly to shared prefs. We do this on
    // a background thread to avoid IO on the main thread and strict mode
    // warnings.
    new Thread(new PersistEngineSelectionsRunnable(engineSelections)).start();
  }

  protected void requestDelayedSync() {
    Logger.info(LOG_TAG, "Posting a delayed request for a sync sometime soon.");
    handler.removeCallbacks(requestSyncRunnable);
    handler.postDelayed(requestSyncRunnable, DELAY_IN_MILLISECONDS_BEFORE_REQUESTING_SYNC);
  }

  /**
   * Remove all traces of debug buttons. By default, no debug buttons are shown.
   */
  protected void removeDebugButtons() {
    final PreferenceScreen statusScreen = (PreferenceScreen) ensureFindPreference("status_screen");
    final PreferenceCategory debugCategory = (PreferenceCategory) ensureFindPreference("debug_category");
    statusScreen.removePreference(debugCategory);
  }

  /**
   * A Runnable that persists engine selections to shared prefs, and then
   * requests a delayed sync.
   * <p>
   * References the member <code>fxAccount</code> and is specific to the Android
   * account associated to that account.
   */
  protected class PersistEngineSelectionsRunnable implements Runnable {
    private final Map<String, Boolean> engineSelections;

    protected PersistEngineSelectionsRunnable(Map<String, Boolean> engineSelections) {
      this.engineSelections = engineSelections;
    }

    @Override
    public void run() {
      try {
        // Name shadowing -- do you like it, or do you love it?
        AndroidFxAccount fxAccount = FxAccountStatusFragment.this.fxAccount;
        if (fxAccount == null) {
          return;
        }
        Logger.info(LOG_TAG, "Persisting engine selections: " + engineSelections.toString());
        SyncConfiguration.storeSelectedEnginesToPrefs(fxAccount.getSyncPrefs(), engineSelections);
        requestDelayedSync();
      } catch (Exception e) {
        Logger.warn(LOG_TAG, "Got exception persisting selected engines; ignoring.", e);
        return;
      }
    }
  }

  /**
   * A Runnable that requests a sync.
   * <p>
   * References the member <code>fxAccount</code>, but is not specific to the
   * Android account associated to that account.
   */
  protected class RequestSyncRunnable implements Runnable {
    @Override
    public void run() {
      // Name shadowing -- do you like it, or do you love it?
      AndroidFxAccount fxAccount = FxAccountStatusFragment.this.fxAccount;
      if (fxAccount == null) {
        return;
      }
      Logger.info(LOG_TAG, "Requesting a sync sometime soon.");
      fxAccount.requestSync();
    }
  }

  /**
   * A separate listener to separate debug logic from main code paths.
   */
  protected class DebugPreferenceClickListener implements OnPreferenceClickListener {
    @Override
    public boolean onPreferenceClick(Preference preference) {
      final String key = preference.getKey();
      if ("debug_refresh".equals(key)) {
        Logger.info(LOG_TAG, "Refreshing.");
        refresh();
      } else if ("debug_dump".equals(key)) {
        fxAccount.dump();
      } else if ("debug_force_sync".equals(key)) {
        Logger.info(LOG_TAG, "Force syncing.");
        fxAccount.requestSync(FirefoxAccounts.FORCE);
        // No sense refreshing, since the sync will complete in the future.
      } else if ("debug_forget_certificate".equals(key)) {
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
      } else if ("debug_require_password".equals(key)) {
        Logger.info(LOG_TAG, "Moving to Separated state: Forgetting password.");
        State state = fxAccount.getState();
        fxAccount.setState(state.makeSeparatedState());
        refresh();
      } else if ("debug_require_upgrade".equals(key)) {
        Logger.info(LOG_TAG, "Moving to Doghouse state: Requiring upgrade.");
        State state = fxAccount.getState();
        fxAccount.setState(state.makeDoghouseState());
        refresh();
      } else {
        return false;
      }
      return true;
    }
  }

  /**
   * Iterate through debug buttons, adding a special debug preference click
   * listener to each of them.
   */
  protected void connectDebugButtons() {
    // Separate listener to really separate debug logic from main code paths.
    final OnPreferenceClickListener listener = new DebugPreferenceClickListener();

    // We don't want to use Android resource strings for debug UI, so we just
    // use the keys throughout.
    final Preference debugCategory = ensureFindPreference("debug_category");
    debugCategory.setTitle(debugCategory.getKey());

    String[] debugKeys = new String[] {
        "debug_refresh",
        "debug_dump",
        "debug_force_sync",
        "debug_forget_certificate",
        "debug_require_password",
        "debug_require_upgrade" };
    for (String debugKey : debugKeys) {
      final Preference button = ensureFindPreference(debugKey);
      button.setTitle(debugKey); // Not very friendly, but this is for debugging only!
      button.setOnPreferenceClickListener(listener);
    }
  }

  @Override
  public boolean onPreferenceChange(Preference preference, Object newValue) {
    if (preference == deviceNamePreference) {
      String newClientName = (String) newValue;
      if (TextUtils.isEmpty(newClientName)) {
        newClientName = clientsDataDelegate.getDefaultClientName();
      }
      final long now = System.currentTimeMillis();
      clientsDataDelegate.setClientName(newClientName, now);
      requestDelayedSync(); // Try to update our remote client record.
      hardRefresh(); // Updates the value displayed to the user, among other things.
      return true;
    }

    // For everything else, accept the change.
    return true;
  }
}
