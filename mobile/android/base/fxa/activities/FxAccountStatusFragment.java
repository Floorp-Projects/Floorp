/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.R;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
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
import org.mozilla.gecko.util.ThreadUtils;

import android.accounts.Account;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
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
import android.support.v4.content.LocalBroadcastManager;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.widget.Toast;

import com.squareup.picasso.Picasso;
import com.squareup.picasso.Target;

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

  /**
   * If a device claims to have synced before this date, we will assume it has never synced.
   */
  private static final Date EARLIEST_VALID_SYNCED_DATE;

  static {
    final Calendar c = GregorianCalendar.getInstance();
    c.set(2000, Calendar.JANUARY, 1, 0, 0, 0);
    EARLIEST_VALID_SYNCED_DATE = c.getTime();
  }

  // When a checkbox is toggled, wait 5 seconds (for other checkbox actions)
  // before trying to sync. Should we kill off the fragment before the sync
  // request happens, that's okay: the runnable will run if the UI thread is
  // still around to service it, and since we're not updating any UI, we'll just
  // schedule the sync as usual. See also comment below about garbage
  // collection.
  private static final long DELAY_IN_MILLISECONDS_BEFORE_REQUESTING_SYNC = 5 * 1000;
  private static final long LAST_SYNCED_TIME_UPDATE_INTERVAL_IN_MILLISECONDS = 60 * 1000;
  private static final long PROFILE_FETCH_RETRY_INTERVAL_IN_MILLISECONDS = 60 * 1000;

  // By default, the auth/account server preference is only shown when the
  // account is configured to use a custom server. In debug mode, this is set.
  private static boolean ALWAYS_SHOW_AUTH_SERVER = false;

  // By default, the Sync server preference is only shown when the account is
  // configured to use a custom Sync server. In debug mode, this is set.
  private static boolean ALWAYS_SHOW_SYNC_SERVER = false;

  // If the user clicks the email field this many times, the debug / personal
  // information logging setting will toggle. The setting is not permanent: it
  // lasts until this process is killed. We don't want to dump PII to the log
  // for a long time!
  private final int NUMBER_OF_CLICKS_TO_TOGGLE_DEBUG =
      // !defined(MOZILLA_OFFICIAL) || defined(NIGHTLY_BUILD) || defined(MOZ_DEBUG)
      (!AppConstants.MOZILLA_OFFICIAL || AppConstants.NIGHTLY_BUILD || AppConstants.DEBUG_BUILD) ? 5 : -1 /* infinite */;
  private int debugClickCount = 0;

  protected PreferenceCategory accountCategory;
  protected Preference profilePreference;
  protected Preference emailPreference;
  protected Preference authServerPreference;

  protected Preference needsPasswordPreference;
  protected Preference needsUpgradePreference;
  protected Preference needsVerificationPreference;
  protected Preference needsMasterSyncAutomaticallyEnabledPreference;
  protected Preference needsFinishMigratingPreference;

  protected PreferenceCategory syncCategory;

  protected CheckBoxPreference bookmarksPreference;
  protected CheckBoxPreference historyPreference;
  protected CheckBoxPreference tabsPreference;
  protected CheckBoxPreference passwordsPreference;
  protected CheckBoxPreference readingListPreference;

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

  // Runnable to update last synced time.
  protected Runnable lastSyncedTimeUpdateRunnable;

  // Broadcast Receiver to update profile Information.
  protected FxAccountProfileInformationReceiver accountProfileInformationReceiver;

  protected final InnerSyncStatusDelegate syncStatusDelegate = new InnerSyncStatusDelegate();
  private Target profileAvatarTarget;

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
    profilePreference = ensureFindPreference("profile");
    emailPreference = ensureFindPreference("email");
    if (AppConstants.MOZ_ANDROID_FIREFOX_ACCOUNT_PROFILES) {
      accountCategory.removePreference(emailPreference);
    } else {
      accountCategory.removePreference(profilePreference);
    }
    authServerPreference = ensureFindPreference("auth_server");

    needsPasswordPreference = ensureFindPreference("needs_credentials");
    needsUpgradePreference = ensureFindPreference("needs_upgrade");
    needsVerificationPreference = ensureFindPreference("needs_verification");
    needsMasterSyncAutomaticallyEnabledPreference = ensureFindPreference("needs_master_sync_automatically_enabled");
    needsFinishMigratingPreference = ensureFindPreference("needs_finish_migrating");

    syncCategory = (PreferenceCategory) ensureFindPreference("sync_category");

    bookmarksPreference = (CheckBoxPreference) ensureFindPreference("bookmarks");
    historyPreference = (CheckBoxPreference) ensureFindPreference("history");
    tabsPreference = (CheckBoxPreference) ensureFindPreference("tabs");
    passwordsPreference = (CheckBoxPreference) ensureFindPreference("passwords");

    if (!FxAccountUtils.LOG_PERSONAL_INFORMATION) {
      removeDebugButtons();
    } else {
      connectDebugButtons();
      ALWAYS_SHOW_AUTH_SERVER = true;
      ALWAYS_SHOW_SYNC_SERVER = true;
    }

    if (AppConstants.MOZ_ANDROID_FIREFOX_ACCOUNT_PROFILES) {
      profilePreference.setOnPreferenceClickListener(this);
    } else {
      emailPreference.setOnPreferenceClickListener(this);
    }

    needsPasswordPreference.setOnPreferenceClickListener(this);
    needsVerificationPreference.setOnPreferenceClickListener(this);
    needsFinishMigratingPreference.setOnPreferenceClickListener(this);

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
    final Preference personalInformationPreference = AppConstants.MOZ_ANDROID_FIREFOX_ACCOUNT_PROFILES ? profilePreference : emailPreference;
    if (preference == personalInformationPreference) {
      debugClickCount += 1;
      if (NUMBER_OF_CLICKS_TO_TOGGLE_DEBUG > 0 && debugClickCount >= NUMBER_OF_CLICKS_TO_TOGGLE_DEBUG) {
        debugClickCount = 0;
        FxAccountUtils.LOG_PERSONAL_INFORMATION = !FxAccountUtils.LOG_PERSONAL_INFORMATION;
        Toast.makeText(getActivity(), "Toggled logging Firefox Account personal information!", Toast.LENGTH_LONG).show();
        hardRefresh(); // Display or hide debug options.
      }
      return true;
    }

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

    if (preference == needsFinishMigratingPreference) {
      final Intent intent = new Intent(getActivity(), FxAccountFinishMigratingActivity.class);
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
    services.put(FxAccountAbstractSetupActivity.JSON_KEY_PROFILE, fxAccount.getProfileServerURI());
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
        this.needsFinishMigratingPreference,
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
    needsMasterSyncAutomaticallyEnabledPreference.setTitle(AppConstants.Versions.preLollipop ?
                                                   R.string.fxaccount_status_needs_master_sync_automatically_enabled :
                                                   R.string.fxaccount_status_needs_master_sync_automatically_enabled_v21);
    showOnlyOneErrorPreference(needsMasterSyncAutomaticallyEnabledPreference);
    setCheckboxesEnabled(false);
  }

  protected void showNeedsFinishMigrating() {
    syncCategory.setTitle(R.string.fxaccount_status_sync);
    showOnlyOneErrorPreference(needsFinishMigratingPreference);
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
    lastSyncedTimeUpdateRunnable = new LastSyncTimeUpdateRunnable();

    // We would very much like register these status observers in bookended
    // onResume/onPause calls, but because the Fragment gets onResume during the
    // Activity's super.onResume, it hasn't yet been told its Firefox Account.
    // So we register the observer here (and remove it in onPause), and open
    // ourselves to the possibility that we don't have properly paired
    // register/unregister calls.
    FxAccountSyncStatusHelper.getInstance().startObserving(syncStatusDelegate);

    if (AppConstants.MOZ_ANDROID_FIREFOX_ACCOUNT_PROFILES) {
      // Register a local broadcast receiver to get profile cached notification.
      final IntentFilter intentFilter = new IntentFilter();
      intentFilter.addAction(FxAccountConstants.ACCOUNT_PROFILE_JSON_UPDATED_ACTION);
      accountProfileInformationReceiver = new FxAccountProfileInformationReceiver();
      LocalBroadcastManager.getInstance(getActivity()).registerReceiver(accountProfileInformationReceiver, intentFilter);

      // profilePreference is set during onCreate, so it's definitely not null here.
      final float cornerRadius = getResources().getDimension(R.dimen.fxaccount_profile_image_width) / 2;
      profileAvatarTarget = new PicassoPreferenceIconTarget(getResources(), profilePreference, cornerRadius);
    }

    refresh();
  }

  @Override
  public void onPause() {
    super.onPause();
    FxAccountSyncStatusHelper.getInstance().stopObserving(syncStatusDelegate);

    // Focus lost, remove scheduled update if any.
    if (lastSyncedTimeUpdateRunnable != null) {
      handler.removeCallbacks(lastSyncedTimeUpdateRunnable);
    }

    // Focus lost, unregister broadcast receiver.
    if (accountProfileInformationReceiver != null) {
      LocalBroadcastManager.getInstance(getActivity()).unregisterReceiver(accountProfileInformationReceiver);
    }

    if (profileAvatarTarget != null) {
      Picasso.with(getActivity()).cancelRequest(profileAvatarTarget);
      profileAvatarTarget = null;
    }
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

    updateProfileInformation();
    updateAuthServerPreference();
    updateSyncServerPreference();

    try {
      // There are error states determined by Android, not the login state
      // machine, and we have a chance to present these states here. We handle
      // them specially, since we can't surface these states as part of syncing,
      // because they generally stop syncs from happening regularly. Right now
      // there are no such states.

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
      case NeedsFinishMigrating:
        showNeedsFinishMigrating();
        break;
      case None:
        showConnected();
        break;
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
    if (new Date(startTime).before(EARLIEST_VALID_SYNCED_DATE)) {
      return getActivity().getString(R.string.fxaccount_status_never_synced);
    }
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
    scheduleAndUpdateLastSyncedTime();
  }

  private void updateProfileInformation() {
    if (!AppConstants.MOZ_ANDROID_FIREFOX_ACCOUNT_PROFILES) {
      // Life is so much simpler.
      emailPreference.setTitle(fxAccount.getEmail());
      return;
    }

    final ExtendedJSONObject profileJSON = fxAccount.getProfileJSON();
    if (profileJSON == null) {
      // Update the profile title with email as the fallback.
      // Profile icon by default use the default avatar as the fallback.
      profilePreference.setTitle(fxAccount.getEmail());
      return;
    }

    updateProfileInformation(profileJSON);
  }

  /**
   * Update profile information from json on UI thread.
   *
   * @param profileJSON json fetched from server.
   */
  protected void updateProfileInformation(final ExtendedJSONObject profileJSON) {
    // View changes must always be done on UI thread.
    ThreadUtils.assertOnUiThread();

    FxAccountUtils.pii(LOG_TAG, "Profile JSON is: " + profileJSON.toJSONString());

    final String userName = profileJSON.getString(FxAccountConstants.KEY_PROFILE_JSON_USERNAME);
    // Update the profile username and email if available.
    if (!TextUtils.isEmpty(userName)) {
      profilePreference.setTitle(userName);
      profilePreference.setSummary(fxAccount.getEmail());
    } else {
      profilePreference.setTitle(fxAccount.getEmail());
    }

    // Icon update from java is not supported prior to API 11, skip the avatar image fetch and update for older device.
    if (!AppConstants.Versions.feature11Plus) {
      Logger.info(LOG_TAG, "Skipping profile image fetch for older pre-API 11 devices.");
      return;
    }

    // Avatar URI empty, skip profile image fetch.
    final String avatarURI = profileJSON.getString(FxAccountConstants.KEY_PROFILE_JSON_AVATAR);
    if (TextUtils.isEmpty(avatarURI)) {
      Logger.info(LOG_TAG, "AvatarURI is empty, skipping profile image fetch.");
      return;
    }

    // Using noPlaceholder would avoid a pop of the default image, but it's not available in the version of Picasso
    // we ship in the tree.
    Picasso
        .with(getActivity())
        .load(avatarURI)
        .centerInside()
        .resizeDimen(R.dimen.fxaccount_profile_image_width, R.dimen.fxaccount_profile_image_height)
        .placeholder(R.drawable.sync_avatar_default)
        .error(R.drawable.sync_avatar_default)
        .into(profileAvatarTarget);
  }

  private void scheduleAndUpdateLastSyncedTime() {
    final String lastSynced = getLastSyncedString(fxAccount.getLastSyncedTimestamp());
    syncNowPreference.setSummary(lastSynced);
    handler.postDelayed(lastSyncedTimeUpdateRunnable, LAST_SYNCED_TIME_UPDATE_INTERVAL_IN_MILLISECONDS);
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
   * The Runnable that schedules a future update and updates the last synced time.
   */
  protected class LastSyncTimeUpdateRunnable implements Runnable  {
    @Override
    public void run() {
      scheduleAndUpdateLastSyncedTime();
    }
  }

  /**
   * Broadcast receiver to receive updates for the cached profile action.
   */
  public class FxAccountProfileInformationReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
      if (!intent.getAction().equals(FxAccountConstants.ACCOUNT_PROFILE_JSON_UPDATED_ACTION)) {
        return;
      }

      Logger.info(LOG_TAG, "Profile avatar cache update action broadcast received.");
      // Update the UI from cached profile json on the main thread.
      getActivity().runOnUiThread(new Runnable() {
        @Override
        public void run() {
          updateProfileInformation();
        }
      });
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
      } else if ("debug_invalidate_certificate".equals(key)) {
        State state = fxAccount.getState();
        try {
          Married married = (Married) state;
          Logger.info(LOG_TAG, "Invalidating certificate.");
          fxAccount.setState(married.makeCohabitingState().withCertificate("INVALID CERTIFICATE"));
          refresh();
        } catch (ClassCastException e) {
          Logger.info(LOG_TAG, "Not in Married state; can't invalidate certificate.");
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
      } else if ("debug_migrated_from_sync11".equals(key)) {
        Logger.info(LOG_TAG, "Moving to MigratedFromSync11 state: Requiring password.");
        State state = fxAccount.getState();
        fxAccount.setState(state.makeMigratedFromSync11State(null));
        refresh();
      } else if ("debug_make_account_stage".equals(key)) {
        Logger.info(LOG_TAG, "Moving Account endpoints, in place, to stage.  Deleting Sync and RL prefs and requiring password.");
        fxAccount.unsafeTransitionToStageEndpoints();
        refresh();
      } else if ("debug_make_account_default".equals(key)) {
        Logger.info(LOG_TAG, "Moving Account endpoints, in place, to default (production).  Deleting Sync and RL prefs and requiring password.");
        fxAccount.unsafeTransitionToDefaultEndpoints();
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
    final PreferenceCategory debugCategory = (PreferenceCategory) ensureFindPreference("debug_category");
    debugCategory.setTitle(debugCategory.getKey());

    for (int i = 0; i < debugCategory.getPreferenceCount(); i++) {
      final Preference button = debugCategory.getPreference(i);
      button.setTitle(button.getKey()); // Not very friendly, but this is for debugging only!
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
