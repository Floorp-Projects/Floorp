/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import org.json.JSONArray;
import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.AdjustConstants;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.BrowserLocaleManager;
import org.mozilla.gecko.DataReportingNotification;
import org.mozilla.gecko.DynamicToolbar;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.Experiments;
import org.mozilla.gecko.GeckoActivityStatus;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoApplication;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.LocaleManager;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.R;
import org.mozilla.gecko.SnackbarBuilder;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.TelemetryContract.Method;
import org.mozilla.gecko.activitystream.ActivityStream;
import org.mozilla.gecko.db.BrowserContract.SuggestedSites;
import org.mozilla.gecko.feeds.FeedService;
import org.mozilla.gecko.feeds.action.CheckForUpdatesAction;
import org.mozilla.gecko.permissions.Permissions;
import org.mozilla.gecko.restrictions.Restrictable;
import org.mozilla.gecko.restrictions.Restrictions;
import org.mozilla.gecko.tabqueue.TabQueueHelper;
import org.mozilla.gecko.tabqueue.TabQueuePrompt;
import org.mozilla.gecko.updater.UpdateService;
import org.mozilla.gecko.updater.UpdateServiceHelper;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.ContextUtils;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.InputOptionsUtils;
import org.mozilla.gecko.util.IntentUtils;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.ViewUtil;

import android.annotation.TargetApi;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.Fragment;
import android.app.FragmentManager;
import android.app.NotificationManager;
import android.content.ContentResolver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.content.res.Configuration;
import android.Manifest;
import android.os.Build;
import android.os.Bundle;
import android.preference.CheckBoxPreference;
import android.preference.EditTextPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceActivity;
import android.preference.PreferenceGroup;
import android.preference.SwitchPreference;
import android.preference.TwoStatePreference;
import android.support.design.widget.Snackbar;
import android.support.design.widget.TextInputLayout;
import android.support.v4.content.LocalBroadcastManager;
import android.support.v7.app.ActionBar;
import android.text.Editable;
import android.text.InputType;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.Log;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListAdapter;
import android.widget.ListView;

import org.mozilla.gecko.switchboard.SwitchBoard;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.Map;

public class GeckoPreferences
    extends AppCompatPreferenceActivity
    implements BundleEventListener,
               GeckoActivityStatus,
               OnPreferenceChangeListener,
               OnSharedPreferenceChangeListener
{
    private static final String LOGTAG = "GeckoPreferences";

    // We have a white background, which makes transitions on
    // some devices look bad. Don't use transitions on those
    // devices.
    private static final boolean NO_TRANSITIONS = HardwareUtils.IS_KINDLE_DEVICE;
    private static final int NO_SUCH_ID = 0;

    public static final String NON_PREF_PREFIX = "android.not_a_preference.";
    public static final String INTENT_EXTRA_RESOURCES = "resource";
    public static final String PREFS_TRACKING_PROTECTION_PROMPT_SHOWN = NON_PREF_PREFIX + "trackingProtectionPromptShown";
    public static final String PREFS_HEALTHREPORT_UPLOAD_ENABLED = NON_PREF_PREFIX + "healthreport.uploadEnabled";
    public static final String PREFS_SYNC = NON_PREF_PREFIX + "sync";

    // Has this activity recently started another Gecko activity?
    private boolean mGeckoActivityOpened;

    private static boolean sIsCharEncodingEnabled;
    private boolean mInitialized;
    private PrefsHelper.PrefHandler mPrefsRequest;
    private List<Header> mHeaders;

    // These match keys in resources/xml*/preferences*.xml
    private static final String PREFS_SEARCH_RESTORE_DEFAULTS = NON_PREF_PREFIX + "search.restore_defaults";
    private static final String PREFS_DATA_REPORTING_PREFERENCES = NON_PREF_PREFIX + "datareporting.preferences";
    private static final String PREFS_TELEMETRY_ENABLED = "toolkit.telemetry.enabled";
    private static final String PREFS_CRASHREPORTER_ENABLED = "datareporting.crashreporter.submitEnabled";
    private static final String PREFS_MENU_CHAR_ENCODING = "browser.menu.showCharacterEncoding";
    private static final String PREFS_MP_ENABLED = "privacy.masterpassword.enabled";
    private static final String PREFS_UPDATER_AUTODOWNLOAD = "app.update.autodownload";
    private static final String PREFS_UPDATER_URL = "app.update.url.android";
    private static final String PREFS_GEO_REPORTING = NON_PREF_PREFIX + "app.geo.reportdata";
    private static final String PREFS_GEO_LEARN_MORE = NON_PREF_PREFIX + "geo.learn_more";
    private static final String PREFS_HEALTHREPORT_LINK = NON_PREF_PREFIX + "healthreport.link";
    private static final String PREFS_DEVTOOLS_REMOTE_USB_ENABLED = "devtools.remote.usb.enabled";
    private static final String PREFS_DEVTOOLS_REMOTE_WIFI_ENABLED = "devtools.remote.wifi.enabled";
    private static final String PREFS_DEVTOOLS_REMOTE_LINK = NON_PREF_PREFIX + "remote_debugging.link";
    private static final String PREFS_TRACKING_PROTECTION = "privacy.trackingprotection.state";
    private static final String PREFS_TRACKING_PROTECTION_PB = "privacy.trackingprotection.pbmode.enabled";
    private static final String PREFS_ZOOMED_VIEW_ENABLED = "ui.zoomedview.enabled";
    public static final String PREFS_VOICE_INPUT_ENABLED = NON_PREF_PREFIX + "voice_input_enabled";
    public static final String PREFS_QRCODE_ENABLED = NON_PREF_PREFIX + "qrcode_enabled";
    private static final String PREFS_TRACKING_PROTECTION_PRIVATE_BROWSING = "privacy.trackingprotection.pbmode.enabled";
    private static final String PREFS_TRACKING_PROTECTION_LEARN_MORE = NON_PREF_PREFIX + "trackingprotection.learn_more";
    private static final String PREFS_CLEAR_PRIVATE_DATA = NON_PREF_PREFIX + "privacy.clear";
    private static final String PREFS_CLEAR_PRIVATE_DATA_EXIT = NON_PREF_PREFIX + "history.clear_on_exit";
    private static final String PREFS_SCREEN_ADVANCED = NON_PREF_PREFIX + "advanced_screen";
    public static final String PREFS_HOMEPAGE = NON_PREF_PREFIX + "homepage";
    public static final String PREFS_HOMEPAGE_PARTNER_COPY = GeckoPreferences.PREFS_HOMEPAGE + ".partner";
    public static final String PREFS_HISTORY_SAVED_SEARCH = NON_PREF_PREFIX + "search.search_history.enabled";
    private static final String PREFS_FAQ_LINK = NON_PREF_PREFIX + "faq.link";
    private static final String PREFS_FEEDBACK_LINK = NON_PREF_PREFIX + "feedback.link";
    public static final String PREFS_NOTIFICATIONS_CONTENT = NON_PREF_PREFIX + "notifications.content";
    public static final String PREFS_NOTIFICATIONS_CONTENT_LEARN_MORE = NON_PREF_PREFIX + "notifications.content.learn_more";
    public static final String PREFS_NOTIFICATIONS_WHATS_NEW = NON_PREF_PREFIX + "notifications.whats_new";
    public static final String PREFS_APP_UPDATE_LAST_BUILD_ID = "app.update.last_build_id";
    public static final String PREFS_READ_PARTNER_CUSTOMIZATIONS_PROVIDER = NON_PREF_PREFIX + "distribution.read_partner_customizations_provider";
    public static final String PREFS_READ_PARTNER_BOOKMARKS_PROVIDER = NON_PREF_PREFIX + "distribution.read_partner_bookmarks_provider";
    public static final String PREFS_CUSTOM_TABS = NON_PREF_PREFIX + "customtabs";
    public static final String PREFS_ACTIVITY_STREAM = NON_PREF_PREFIX + "activitystream";
    public static final String PREFS_CATEGORY_EXPERIMENTAL_FEATURES = NON_PREF_PREFIX + "category_experimental";
    public static final String PREFS_COMPACT_TABS = NON_PREF_PREFIX + "compact_tabs";
    public static final String PREFS_SHOW_QUIT_MENU = NON_PREF_PREFIX + "distribution.show_quit_menu";
    public static final String PREFS_SEARCH_SUGGESTIONS_ENABLED = "browser.search.suggest.enabled";
    public static final String PREFS_DEFAULT_BROWSER = NON_PREF_PREFIX + "default_browser.link";
    public static final String PREFS_SYSTEM_FONT_SIZE = NON_PREF_PREFIX + "font.size.use_system_font_size";

    private static final String ACTION_STUMBLER_UPLOAD_PREF = "STUMBLER_PREF";


    // This isn't a Gecko pref, even if it looks like one.
    private static final String PREFS_BROWSER_LOCALE = "locale";

    public static final String PREFS_RESTORE_SESSION = NON_PREF_PREFIX + "restoreSession3";
    public static final String PREFS_RESTORE_SESSION_FROM_CRASH = "browser.sessionstore.resume_from_crash";
    public static final String PREFS_RESTORE_SESSION_MAX_CRASH_RESUMES = "browser.sessionstore.max_resumed_crashes";
    public static final String PREFS_RESTORE_SESSION_ONCE = "browser.sessionstore.resume_session_once";
    public static final String PREFS_TAB_QUEUE = NON_PREF_PREFIX + "tab_queue";
    public static final String PREFS_TAB_QUEUE_LAST_SITE = NON_PREF_PREFIX + "last_site";
    public static final String PREFS_TAB_QUEUE_LAST_TIME = NON_PREF_PREFIX + "last_time";

    private static final String PREFS_DYNAMIC_TOOLBAR = "browser.chrome.dynamictoolbar";

    // These values are chosen to be distinct from other Activity constants.
    private static final int REQUEST_CODE_PREF_SCREEN = 5;
    private static final int RESULT_CODE_EXIT_SETTINGS = 6;

    // Result code used when a locale preference changes.
    // Callers can recognize this code to refresh themselves to
    // accommodate a locale change.
    public static final int RESULT_CODE_LOCALE_DID_CHANGE = 7;

    private static final int REQUEST_CODE_TAB_QUEUE = 8;

    private final Map<String, PrefHandler> HANDLERS;
    {
        final HashMap<String, PrefHandler> tempHandlers = new HashMap<>(2);
        tempHandlers.put(ClearOnShutdownPref.PREF, new ClearOnShutdownPref());
        tempHandlers.put(AndroidImportPreference.PREF_KEY, new AndroidImportPreference.Handler());
        HANDLERS = Collections.unmodifiableMap(tempHandlers);
    }

    private SwitchPreference tabQueuePreference;

    /**
     * Track the last locale so we know whether to redisplay.
     */
    private Locale lastLocale = Locale.getDefault();
    private boolean localeSwitchingIsEnabled;

    private void startActivityForResultChoosingTransition(final Intent intent, final int requestCode) {
        startActivityForResult(intent, requestCode);
        if (NO_TRANSITIONS) {
            overridePendingTransition(0, 0);
        }
    }

    private void finishChoosingTransition() {
        finish();
        if (NO_TRANSITIONS) {
            overridePendingTransition(0, 0);
        }
    }
    private void updateActionBarTitle(int title) {
        final String newTitle = getString(title);
        if (newTitle != null) {
            Log.v(LOGTAG, "Setting action bar title to " + newTitle);

            setTitle(newTitle);
        }
    }

    private void updateHomeAsUpIndicator() {
        final ActionBar actionBar = getSupportActionBar();
        if (actionBar == null) {
            return;
        }
        actionBar.setHomeAsUpIndicator(android.support.v7.appcompat.R.drawable.abc_ic_ab_back_mtrl_am_alpha);
    }

    /**
     * We only call this method for pre-HC versions of Android.
     */
    private void updateTitleForPrefsResource(int res) {
        // At present we only need to do this for non-leaf prefs views
        // and the locale switcher itself.
        int title = -1;
        if (res == R.xml.preferences) {
            title = R.string.settings_title;
        } else if (res == R.xml.preferences_locale) {
            title = R.string.pref_category_language;
        } else if (res == R.xml.preferences_vendor) {
            title = R.string.pref_category_vendor;
        } else if (res == R.xml.preferences_general) {
            title = R.string.pref_category_general;
        } else if (res == R.xml.preferences_search) {
            title = R.string.pref_category_search;
        }
        if (title != -1) {
            setTitle(title);
        }
    }

    private void onLocaleChanged(Locale newLocale) {
        Log.d(LOGTAG, "onLocaleChanged: " + newLocale);

        BrowserLocaleManager.getInstance().updateConfiguration(getApplicationContext(), newLocale);
        //  If activity is not recreated, also update locale to current activity configuration
        BrowserLocaleManager.getInstance().updateConfiguration(GeckoPreferences.this, newLocale);
        ViewUtil.setLayoutDirection(getWindow().getDecorView(), newLocale);
        //  Force update navigate up icon by current layout direction
        updateHomeAsUpIndicator();

        this.lastLocale = newLocale;

        if (isMultiPane()) {
            // This takes care of the left pane.
            invalidateHeaders();

            // Detach and reattach the current prefs pane so that it
            // reflects the new locale.
            final FragmentManager fragmentManager = getFragmentManager();
            int id = getResources().getIdentifier("android:id/prefs", null, null);
            final Fragment current = fragmentManager.findFragmentById(id);
            if (current != null) {
                fragmentManager.beginTransaction()
                               .disallowAddToBackStack()
                               .detach(current)
                               .attach(current)
                               .commitAllowingStateLoss();
            } else {
                Log.e(LOGTAG, "No prefs fragment to reattach!");
            }

            // Because Android just rebuilt the activity itself with the
            // old language, we need to update the top title and other
            // wording again.
            if (onIsMultiPane()) {
                updateActionBarTitle(R.string.settings_title);
            }

            // Update the title to for the preference pane that we're currently showing.
            final int titleId = getIntent().getExtras().getInt(PreferenceActivity.EXTRA_SHOW_FRAGMENT_TITLE);
            if (titleId != NO_SUCH_ID) {
                setTitle(titleId);
            } else {
                throw new IllegalStateException("Title id not found in intent bundle extras");
            }

            // Don't finish the activity -- we just reloaded all of the
            // individual parts! -- but when it returns, make sure that the
            // caller knows the locale changed.
            setResult(RESULT_CODE_LOCALE_DID_CHANGE);
            return;
        }

        refreshSuggestedSites();

        // Cause the current fragment to redisplay, the hard way.
        // This avoids nonsense with trying to reach inside fragments and force them
        // to redisplay themselves.
        // We also don't need to update the title.
        final Intent intent = (Intent) getIntent().clone();
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
        startActivityForResultChoosingTransition(intent, REQUEST_CODE_PREF_SCREEN);

        setResult(RESULT_CODE_LOCALE_DID_CHANGE);
        finishChoosingTransition();
    }

    private void checkLocale() {
        if (AppConstants.Versions.feature21Plus && AppConstants.Versions.preMarshmallow) {
            //  Force update navigate up icon by current layout direction
            updateHomeAsUpIndicator();
        }

        final Locale currentLocale = Locale.getDefault();
        Log.v(LOGTAG, "Checking locale: " + currentLocale + " vs " + lastLocale);
        if (currentLocale.equals(lastLocale)) {
            return;
        }

        onLocaleChanged(currentLocale);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Apply the current user-selected locale, if necessary.
        checkLocale();

        // Track this so we can decide whether to show locale options.
        // See also the workaround below for Bug 1015209.
        localeSwitchingIsEnabled = BrowserLocaleManager.getInstance().isEnabled();

        // For Android v11+ where we use Fragments (v11+ only due to bug 866352),
        // check that PreferenceActivity.EXTRA_SHOW_FRAGMENT has been set
        // (or set it) before super.onCreate() is called so Android can display
        // the correct Fragment resource.
        // Note: this seems to only be required for non-multipane devices, multipane
        // manages to automatically select the correct fragments.
        if (!getIntent().hasExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT)) {
            // Set up the default fragment if there is no explicit fragment to show.
            setupTopLevelFragmentIntent();
        }

        // We must call this before setTitle to avoid crashes. Most devices don't seem to care
        // (we used to call onCreate later), however the ASUS TF300T (running 4.2) crashes
        // with an NPE in android.support.v7.app.AppCompatDelegateImplV7.ensureSubDecor(), and it's
        // likely other strange devices (other Asus devices, some Samsungs) could do the same.
        super.onCreate(savedInstanceState);

        if (onIsMultiPane()) {
            // So that Android doesn't put the fragment title (or nothing at
            // all) in the action bar.
            updateActionBarTitle(R.string.settings_title);

            if (Build.VERSION.SDK_INT < 13) {
                // Affected by Bug 1015209 -- no detach/attach.
                // If we try rejigging fragments, we'll crash, so don't
                // enable locale switching at all.
                localeSwitchingIsEnabled = false;
                throw new IllegalStateException("foobar");
            }
        }

        // Use setResourceToOpen to specify these extras.
        Bundle intentExtras = getIntent().getExtras();

        EventDispatcher.getInstance().registerUiThreadListener(this, "Sanitize:Finished");

        // Add handling for long-press click.
        // This is only for Android 3.0 and below (which use the long-press-context-menu paradigm).
        final ListView mListView = getListView();
        mListView.setOnItemLongClickListener(new AdapterView.OnItemLongClickListener() {
            @Override
            public boolean onItemLongClick(AdapterView<?> parent, View view, int position, long id) {
                // Call long-click handler if it the item implements it.
                final ListAdapter listAdapter = ((ListView) parent).getAdapter();
                final Object listItem = listAdapter.getItem(position);

                // Only CustomListPreference handles long clicks.
                if (listItem instanceof CustomListPreference && listItem instanceof View.OnLongClickListener) {
                    final View.OnLongClickListener longClickListener = (View.OnLongClickListener) listItem;
                    return longClickListener.onLongClick(view);
                }
                return false;
            }
        });

        // N.B., if we ever need to redisplay the locale selection UI without
        // just finishing and recreating the activity, right here we'll need to
        // capture EXTRA_SHOW_FRAGMENT_TITLE from the intent and store the title ID.

        // If launched from notification, explicitly cancel the notification.
        if (intentExtras != null && intentExtras.containsKey(DataReportingNotification.ALERT_NAME_DATAREPORTING_NOTIFICATION)) {
            Telemetry.sendUIEvent(TelemetryContract.Event.LAUNCH, Method.NOTIFICATION, "settings-data-choices");
            NotificationManager notificationManager = (NotificationManager) this.getSystemService(Context.NOTIFICATION_SERVICE);
            notificationManager.cancel(DataReportingNotification.ALERT_NAME_DATAREPORTING_NOTIFICATION.hashCode());
        }

        // Launched from "Notifications settings" action button in a notification.
        if (intentExtras != null && intentExtras.containsKey(CheckForUpdatesAction.EXTRA_CONTENT_NOTIFICATION)) {
            Telemetry.startUISession(TelemetryContract.Session.EXPERIMENT, FeedService.getEnabledExperiment(this));
            Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, Method.BUTTON, "notification-settings");
            Telemetry.stopUISession(TelemetryContract.Session.EXPERIMENT, FeedService.getEnabledExperiment(this));
        }
    }

    /**
     * Set intent to display top-level settings fragment,
     * and show the correct title.
     */
    private void setupTopLevelFragmentIntent() {
        Intent intent = getIntent();
        // Check intent to determine settings screen to display.
        Bundle intentExtras = intent.getExtras();
        Bundle fragmentArgs = new Bundle();
        // Add resource argument to fragment if it exists.
        if (intentExtras != null && intentExtras.containsKey(INTENT_EXTRA_RESOURCES)) {
            String resourceName = intentExtras.getString(INTENT_EXTRA_RESOURCES);
            fragmentArgs.putString(INTENT_EXTRA_RESOURCES, resourceName);
        } else {
            // Use top-level settings screen.
            if (!onIsMultiPane()) {
                fragmentArgs.putString(INTENT_EXTRA_RESOURCES, "preferences");
            } else {
                fragmentArgs.putString(INTENT_EXTRA_RESOURCES, "preferences_general_tablet");
            }
        }

        // Build fragment intent.
        intent.putExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT, GeckoPreferenceFragment.class.getName());
        intent.putExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArgs);
        // Used to get fragment title when locale changes (see onLocaleChanged method above)
        intent.putExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT_TITLE, R.string.settings_title);
    }

    @Override
    public boolean isValidFragment(String fragmentName) {
        return GeckoPreferenceFragment.class.getName().equals(fragmentName);
    }

    @TargetApi(11)
    @Override
    public void onBuildHeaders(List<Header> target) {
        if (onIsMultiPane()) {
            loadHeadersFromResource(R.xml.preference_headers, target);

            Iterator<Header> iterator = target.iterator();

            while (iterator.hasNext()) {
                Header header = iterator.next();

                if (header.id == R.id.pref_header_advanced && !Restrictions.isAllowed(this, Restrictable.ADVANCED_SETTINGS)) {
                    iterator.remove();
                } else if (header.id == R.id.pref_header_clear_private_data
                           && !Restrictions.isAllowed(this, Restrictable.CLEAR_HISTORY)) {
                    iterator.remove();
                }
            }

            mHeaders = target;
        }
    }

    @TargetApi(11)
    public void switchToHeader(int id) {
        if (mHeaders == null) {
            // Can't switch to a header if there are no headers!
            return;
        }

        for (Header header : mHeaders) {
            if (header.id == id) {
                switchToHeader(header);
                return;
            }
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        if (!hasFocus || mInitialized)
            return;

        mInitialized = true;
    }

    @Override
    public void onBackPressed() {
        super.onBackPressed();

        if (NO_TRANSITIONS) {
            overridePendingTransition(0, 0);
        }

        Telemetry.sendUIEvent(TelemetryContract.Event.CANCEL, Method.BACK, "settings");
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        EventDispatcher.getInstance().unregisterUiThreadListener(this, "Sanitize:Finished");

        if (mPrefsRequest != null) {
            PrefsHelper.removeObserver(mPrefsRequest);
            mPrefsRequest = null;
        }
    }

    @Override
    public void onPause() {
        EventDispatcher.getInstance().unregisterUiThreadListener(this, "Snackbar:Show");

        // Symmetric with onResume.
        if (isMultiPane()) {
            SharedPreferences prefs = GeckoSharedPrefs.forApp(this);
            prefs.unregisterOnSharedPreferenceChangeListener(this);
        }

        super.onPause();

        if (getApplication() instanceof GeckoApplication) {
            ((GeckoApplication) getApplication()).onActivityPause(this);
        }
    }

    @Override
    public void onResume() {
        super.onResume();

        EventDispatcher.getInstance().registerUiThreadListener(this, "Snackbar:Show");

        if (getApplication() instanceof GeckoApplication) {
            ((GeckoApplication) getApplication()).onActivityResume(this);
            mGeckoActivityOpened = false;
        }

        // Watch prefs, otherwise we don't reliably get told when they change.
        // See documentation for onSharedPreferenceChange for more.
        // Inexplicably only needed on tablet.
        if (isMultiPane()) {
            SharedPreferences prefs = GeckoSharedPrefs.forApp(this);
            prefs.registerOnSharedPreferenceChangeListener(this);
        }
    }

    @Override
    public void startActivity(Intent intent) {
        // For settings, we want to be able to pass results up the chain
        // of preference screens so Settings can behave as a single unit.
        // Specifically, when we open a link, we want to back out of all
        // the settings screens.
        // We need to start nested PreferenceScreens withStartActivityForResult().
        // Android doesn't let us do that (see Preference.onClick), so we're overriding here.
        startActivityForResultChoosingTransition(intent, REQUEST_CODE_PREF_SCREEN);
    }

    @Override
    public void startActivityForResult(Intent intent, int request) {
        mGeckoActivityOpened = IntentUtils.checkIfGeckoActivity(intent);
        super.startActivityForResult(intent, request);
    }

    @Override
    public void startWithFragment(String fragmentName, Bundle args,
            Fragment resultTo, int resultRequestCode, int titleRes, int shortTitleRes) {
        Log.v(LOGTAG, "Starting with fragment: " + fragmentName + ", title " + titleRes);

        // Overriding because we want to use startActivityForResult for Fragment intents.
        Intent intent = onBuildStartFragmentIntent(fragmentName, args, titleRes, shortTitleRes);
        if (resultTo == null) {
            startActivityForResultChoosingTransition(intent, REQUEST_CODE_PREF_SCREEN);
        } else {
            mGeckoActivityOpened = IntentUtils.checkIfGeckoActivity(intent);
            resultTo.startActivityForResult(intent, resultRequestCode);
            if (NO_TRANSITIONS) {
                overridePendingTransition(0, 0);
            }
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        // We might have just returned from a settings activity that allows us
        // to switch locales, so reflect any change that occurred.
        checkLocale();

        switch (requestCode) {
          case REQUEST_CODE_PREF_SCREEN:
              switch (resultCode) {
              case RESULT_CODE_EXIT_SETTINGS:
                  updateActionBarTitle(R.string.settings_title);

                  // Pass this result up to the parent activity.
                  setResult(RESULT_CODE_EXIT_SETTINGS);
                  finishChoosingTransition();
                  break;
              }
              break;
            case REQUEST_CODE_TAB_QUEUE:
                if (TabQueueHelper.processTabQueuePromptResponse(resultCode, this)) {
                    tabQueuePreference.setChecked(true);
                }
                break;
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        Permissions.onRequestPermissionsResult(this, permissions, grantResults);
    }

    @Override
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if ("Snackbar:Show".equals(event)) {
            SnackbarBuilder.builder(this)
                    .fromEvent(message)
                    .callback(callback)
                    .buildAndShow();

        } else if ("Sanitize:Finished".equals(event)) {
            final boolean success = message.getBoolean("success");
            final int stringRes = success ? R.string.private_data_success : R.string.private_data_fail;

            SnackbarBuilder.builder(this)
                    .message(stringRes)
                    .duration(Snackbar.LENGTH_LONG)
                    .buildAndShow();
        }
    }

    /**
      * Initialize all of the preferences (native of Gecko ones) for this screen.
      *
      * @param prefs The android.preference.PreferenceGroup to initialize
      * @return The integer id for the PrefsHelper.PrefHandlerBase listener added
      *         to monitor changes to Gecko prefs.
      */
    public PrefsHelper.PrefHandler setupPreferences(PreferenceGroup prefs) {
        ArrayList<String> list = new ArrayList<String>();
        setupPreferences(prefs, list);
        return getGeckoPreferences(prefs, list);
    }

    /**
      * Recursively loop through a PreferenceGroup. Initialize native Android prefs,
      * and build a list of Gecko preferences in the passed in prefs array
      *
      * @param preferences The android.preference.PreferenceGroup to initialize
      * @param prefs An ArrayList to fill with Gecko preferences that need to be
      *        initialized
      * @return The integer id for the PrefsHelper.PrefHandlerBase listener added
      *         to monitor changes to Gecko prefs.
      */
    private void setupPreferences(PreferenceGroup preferences, ArrayList<String> prefs) {
        for (int i = 0; i < preferences.getPreferenceCount(); i++) {
            final Preference pref = preferences.getPreference(i);

            // Eliminate locale switching if necessary.
            // This logic will need to be extended when
            // content language selection (Bug 881510) is implemented.
            if (!localeSwitchingIsEnabled &&
                "preferences_locale".equals(pref.getExtras().getString("resource"))) {
                preferences.removePreference(pref);
                i--;
                continue;
            }

            String key = pref.getKey();
            if (pref instanceof PreferenceGroup) {
                // If datareporting is disabled, remove UI.
                if (PREFS_DATA_REPORTING_PREFERENCES.equals(key)) {
                    if (!AppConstants.MOZ_DATA_REPORTING || !Restrictions.isAllowed(this, Restrictable.DATA_CHOICES)) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_SCREEN_ADVANCED.equals(key) &&
                        !Restrictions.isAllowed(this, Restrictable.ADVANCED_SETTINGS)) {
                    preferences.removePreference(pref);
                    i--;
                    continue;
                } else if (PREFS_CATEGORY_EXPERIMENTAL_FEATURES.equals(key)
                        && !AppConstants.MOZ_ANDROID_ACTIVITY_STREAM
                        && !AppConstants.MOZ_ANDROID_CUSTOM_TABS) {
                    preferences.removePreference(pref);
                    i--;
                    continue;
                }
                setupPreferences((PreferenceGroup) pref, prefs);
            } else {
                if (HANDLERS.containsKey(key)) {
                    PrefHandler handler = HANDLERS.get(key);
                    if (!handler.setupPref(this, pref)) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                }

                pref.setOnPreferenceChangeListener(this);
                if (PREFS_UPDATER_AUTODOWNLOAD.equals(key)) {
                    if (!AppConstants.MOZ_UPDATER || ContextUtils.isInstalledFromGooglePlay(this)) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_TRACKING_PROTECTION.equals(key)) {
                    // Remove UI for global TP pref in non-Nightly builds.
                    if (!AppConstants.NIGHTLY_BUILD) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_TRACKING_PROTECTION_PB.equals(key)) {
                    // Remove UI for private-browsing-only TP pref in Nightly builds.
                    if (AppConstants.NIGHTLY_BUILD) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_TELEMETRY_ENABLED.equals(key)) {
                    if (!AppConstants.MOZ_TELEMETRY_REPORTING || !Restrictions.isAllowed(this, Restrictable.DATA_CHOICES)) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_HEALTHREPORT_UPLOAD_ENABLED.equals(key) ||
                           PREFS_HEALTHREPORT_LINK.equals(key)) {
                    if (!AppConstants.MOZ_SERVICES_HEALTHREPORT || !Restrictions.isAllowed(this, Restrictable.DATA_CHOICES)) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_CRASHREPORTER_ENABLED.equals(key)) {
                    if (!AppConstants.MOZ_CRASHREPORTER || !Restrictions.isAllowed(this, Restrictable.DATA_CHOICES)) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_GEO_REPORTING.equals(key) ||
                           PREFS_GEO_LEARN_MORE.equals(key)) {
                    if (!AppConstants.MOZ_STUMBLER_BUILD_TIME_ENABLED || !Restrictions.isAllowed(this, Restrictable.DATA_CHOICES)) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_DEVTOOLS_REMOTE_USB_ENABLED.equals(key)) {
                    if (!Restrictions.isAllowed(this, Restrictable.REMOTE_DEBUGGING)) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_DEVTOOLS_REMOTE_WIFI_ENABLED.equals(key)) {
                    if (!Restrictions.isAllowed(this, Restrictable.REMOTE_DEBUGGING)) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                    if (!InputOptionsUtils.supportsQrCodeReader(getApplicationContext())) {
                        // WiFi debugging requires a QR code reader
                        pref.setEnabled(false);
                        pref.setSummary(getString(R.string.pref_developer_remotedebugging_wifi_disabled_summary));
                        continue;
                    }
                } else if (PREFS_DEVTOOLS_REMOTE_LINK.equals(key)) {
                    // Remove the "Learn more" link if remote debugging is disabled
                    if (!Restrictions.isAllowed(this, Restrictable.REMOTE_DEBUGGING)) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_RESTORE_SESSION.equals(key) ||
                           PREFS_BROWSER_LOCALE.equals(key)) {
                    // Set the summary string to the current entry. The summary
                    // for other list prefs will be set in the PrefsHelper
                    // callback, but since this pref doesn't live in Gecko, we
                    // need to handle it separately.
                    ListPreference listPref = (ListPreference) pref;
                    CharSequence selectedEntry = listPref.getEntry();
                    listPref.setSummary(selectedEntry);
                    continue;
                } else if (PREFS_SYNC.equals(key)) {
                    // Don't show sync prefs while in guest mode.
                    if (!Restrictions.isAllowed(this, Restrictable.MODIFY_ACCOUNTS)) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_SEARCH_RESTORE_DEFAULTS.equals(key)) {
                    pref.setOnPreferenceClickListener(new OnPreferenceClickListener() {
                        @Override
                        public boolean onPreferenceClick(Preference preference) {
                            GeckoPreferences.this.restoreDefaultSearchEngines();
                            Telemetry.sendUIEvent(TelemetryContract.Event.SEARCH_RESTORE_DEFAULTS, Method.LIST_ITEM);
                            return true;
                        }
                    });
                } else if (PREFS_TAB_QUEUE.equals(key)) {
                    tabQueuePreference = (SwitchPreference) pref;
                    // Only show tab queue pref on nightly builds with the tab queue build flag.
                    if (!TabQueueHelper.TAB_QUEUE_ENABLED) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_ZOOMED_VIEW_ENABLED.equals(key)) {
                    if (!AppConstants.NIGHTLY_BUILD) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_VOICE_INPUT_ENABLED.equals(key)) {
                    if (!InputOptionsUtils.supportsVoiceRecognizer(getApplicationContext(), getResources().getString(R.string.voicesearch_prompt))) {
                        // Remove UI for voice input on non nightly builds.
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_QRCODE_ENABLED.equals(key)) {
                    if (!InputOptionsUtils.supportsQrCodeReader(getApplicationContext())) {
                        // Remove UI for qr code input on non nightly builds
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_TRACKING_PROTECTION_PRIVATE_BROWSING.equals(key)) {
                    if (!Restrictions.isAllowed(this, Restrictable.PRIVATE_BROWSING)) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_TRACKING_PROTECTION_LEARN_MORE.equals(key)) {
                    if (!Restrictions.isAllowed(this, Restrictable.PRIVATE_BROWSING)) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_MP_ENABLED.equals(key)) {
                    if (!Restrictions.isAllowed(this, Restrictable.MASTER_PASSWORD)) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_CLEAR_PRIVATE_DATA.equals(key) || PREFS_CLEAR_PRIVATE_DATA_EXIT.equals(key)) {
                    if (!Restrictions.isAllowed(this, Restrictable.CLEAR_HISTORY)) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_HOMEPAGE.equals(key)) {
                        String setUrl = GeckoSharedPrefs.forProfile(getBaseContext()).getString(PREFS_HOMEPAGE, AboutPages.HOME);
                        setHomePageSummary(pref, setUrl);
                        pref.setOnPreferenceChangeListener(this);
                } else if (PREFS_FAQ_LINK.equals(key)) {
                    // Format the FAQ link
                    final String VERSION = AppConstants.MOZ_APP_VERSION;
                    final String OS = AppConstants.OS_TARGET;
                    final String LOCALE = Locales.getLanguageTag(Locale.getDefault());

                    final String url = getResources().getString(R.string.faq_link, VERSION, OS, LOCALE);
                    ((LinkPreference) pref).setUrl(url);
                } else if (PREFS_FEEDBACK_LINK.equals(key)) {
                    // Format the feedback link. We can't easily use this "app.feedbackURL"
                    // Gecko preference because the URL must be formatted.
                    final String url = getResources().getString(R.string.feedback_link, AppConstants.MOZ_APP_VERSION, AppConstants.MOZ_UPDATE_CHANNEL);
                    ((LinkPreference) pref).setUrl(url);
                } else if (PREFS_DYNAMIC_TOOLBAR.equals(key)) {
                    if (DynamicToolbar.isForceDisabled()) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_NOTIFICATIONS_CONTENT.equals(key) ||
                        PREFS_NOTIFICATIONS_CONTENT_LEARN_MORE.equals(key)) {
                    if (!FeedService.isInExperiment(this)) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_CUSTOM_TABS.equals(key) && !AppConstants.MOZ_ANDROID_CUSTOM_TABS) {
                    preferences.removePreference(pref);
                    i--;
                    continue;
                } else if (PREFS_ACTIVITY_STREAM.equals(key) && !ActivityStream.isUserEligible(this)) {
                    preferences.removePreference(pref);
                    i--;
                    continue;
                } else if (PREFS_COMPACT_TABS.equals(key)) {
                    if (HardwareUtils.isTablet()) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    } else {
                        final boolean value = GeckoSharedPrefs.forApp(this).getBoolean(GeckoPreferences.PREFS_COMPACT_TABS,
                                SwitchBoard.isInExperiment(this, Experiments.COMPACT_TABS));

                        pref.setDefaultValue(value);
                        ((SwitchPreference) pref).setChecked(value);
                    }
                }

                // Some Preference UI elements are not actually preferences,
                // but they require a key to work correctly. For example,
                // "Clear private data" requires a key for its state to be
                // saved when the orientation changes. It uses the
                // "android.not_a_preference.privacy.clear" key - which doesn't
                // exist in Gecko - to satisfy this requirement.
                if (isGeckoPref(key)) {
                    prefs.add(key);
                }
            }
        }
    }

    private void setHomePageSummary(Preference pref, String value) {
        if (!TextUtils.isEmpty(value)) {
            pref.setSummary(value);
        } else {
            pref.setSummary(AboutPages.HOME);
        }
    }

    private boolean isGeckoPref(String key) {
        if (TextUtils.isEmpty(key)) {
            return false;
        }

        if (key.startsWith(NON_PREF_PREFIX)) {
            return false;
        }

        if (key.equals(PREFS_BROWSER_LOCALE)) {
            return false;
        }

        return true;
    }

    /**
     * Restore default search engines in Gecko and retrigger a search engine refresh.
     */
    protected void restoreDefaultSearchEngines() {
        EventDispatcher.getInstance().dispatch("SearchEngines:RestoreDefaults", null);

        // Send message to Gecko to get engines. SearchPreferenceCategory listens for the response.
        EventDispatcher.getInstance().dispatch("SearchEngines:GetVisible", null);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int itemId = item.getItemId();
        switch (itemId) {
            case android.R.id.home:
                finishChoosingTransition();
                return true;
        }

        // Generated R.id.* apparently aren't constant expressions, so they can't be switched.
        if (itemId == R.id.restore_defaults) {
            restoreDefaultSearchEngines();
            Telemetry.sendUIEvent(TelemetryContract.Event.SEARCH_RESTORE_DEFAULTS, Method.MENU);
            return true;
       }

        return super.onOptionsItemSelected(item);
    }

    final private int DIALOG_CREATE_MASTER_PASSWORD = 0;
    final private int DIALOG_REMOVE_MASTER_PASSWORD = 1;

    public static void setCharEncodingState(boolean enabled) {
        sIsCharEncodingEnabled = enabled;
    }

    public static boolean getCharEncodingState() {
        return sIsCharEncodingEnabled;
    }

    public static void broadcastAction(final Context context, final Intent intent) {
        fillIntentWithProfileInfo(context, intent);
        LocalBroadcastManager.getInstance(context).sendBroadcast(intent);
    }

    private static void fillIntentWithProfileInfo(final Context context, final Intent intent) {
        // There is a race here, but GeckoProfile returns the default profile
        // when Gecko is not explicitly running for a different profile.  In a
        // multi-profile world, this will need to be updated (possibly to
        // broadcast settings for all profiles).  See Bug 882182.
        GeckoProfile profile = GeckoProfile.get(context);
        if (profile != null) {
            intent.putExtra("profileName", profile.getName())
                  .putExtra("profilePath", profile.getDir().getAbsolutePath());
        }
    }

    /**
     * Broadcast the provided value as the value of the
     * <code>PREFS_GEO_REPORTING</code> pref.
     */
    public static void broadcastStumblerPref(final Context context, final boolean value) {
       Intent intent = new Intent(ACTION_STUMBLER_UPLOAD_PREF)
                .putExtra("pref", PREFS_GEO_REPORTING)
                .putExtra("branch", GeckoSharedPrefs.APP_PREFS_NAME)
                .putExtra("enabled", value)
                .putExtra("moz_mozilla_api_key", AppConstants.MOZ_MOZILLA_API_KEY);
       if (GeckoAppShell.getGeckoInterface() != null) {
           intent.putExtra("user_agent", GeckoAppShell.getGeckoInterface().getDefaultUAString());
       }
       broadcastAction(context, intent);
    }

    /**
     * Broadcast the current value of the
     * <code>PREFS_GEO_REPORTING</code> pref.
     */
    public static void broadcastStumblerPref(final Context context) {
        final boolean value = getBooleanPref(context, PREFS_GEO_REPORTING, false);
        broadcastStumblerPref(context, value);
    }

    /**
     * Return the value of the named preference in the default preferences file.
     *
     * This corresponds to the storage that backs preferences.xml.
     * @param context a <code>Context</code>; the
     *                <code>PreferenceActivity</code> will suffice, but this
     *                method is intended to be called from other contexts
     *                within the application, not just this <code>Activity</code>.
     * @param name    the name of the preference to retrieve.
     * @param def     the default value to return if the preference is not present.
     * @return        the value of the preference, or the default.
     */
    public static boolean getBooleanPref(final Context context, final String name, boolean def) {
        final SharedPreferences prefs = GeckoSharedPrefs.forApp(context);
        return prefs.getBoolean(name, def);
    }

    /**
     * Immediately handle the user's selection of a browser locale.
     *
     * Earlier locale-handling code did this with centralized logic in
     * GeckoApp, delegating to LocaleManager for persistence and refreshing
     * the activity as necessary.
     *
     * We no longer handle this by sending a message to GeckoApp, for
     * several reasons:
     *
     * * GeckoApp might not be running. Activities don't always stick around.
     *   A Java bridge message might not be handled.
     * * We need to adapt the preferences UI to the locale ourselves.
     * * The user might not hit Back (or Up) -- they might hit Home and never
     *   come back.
     *
     * We handle the case of the user returning to the browser via the
     * onActivityResult mechanism: see {@link BrowserApp#onActivityResult(int, int, Intent)}.
     */
    private boolean onLocaleSelected(final String currentLocale, final String newValue) {
        final Context context = getApplicationContext();

        // LocaleManager operations need to occur on the background thread.
        // ... but activity operations need to occur on the UI thread. So we
        // have nested runnables.
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final LocaleManager localeManager = BrowserLocaleManager.getInstance();

                if (TextUtils.isEmpty(newValue)) {
                    BrowserLocaleManager.getInstance().resetToSystemLocale(context);
                    Telemetry.sendUIEvent(TelemetryContract.Event.LOCALE_BROWSER_RESET);
                } else {
                    if (null == localeManager.setSelectedLocale(context, newValue)) {
                        localeManager.updateConfiguration(context, Locale.getDefault());
                    }
                    Telemetry.sendUIEvent(TelemetryContract.Event.LOCALE_BROWSER_UNSELECTED, Method.NONE,
                                          currentLocale == null ? "unknown" : currentLocale);
                    Telemetry.sendUIEvent(TelemetryContract.Event.LOCALE_BROWSER_SELECTED, Method.NONE, newValue);
                }

                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        onLocaleChanged(Locale.getDefault());
                    }
                });
            }
        });

        return true;
    }

    private void refreshSuggestedSites() {
        final ContentResolver cr = getApplicationContext().getContentResolver();

        // This will force all active suggested sites cursors
        // to request a refresh (e.g. cursor loaders).
        cr.notifyChange(SuggestedSites.CONTENT_URI, null);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        Log.d(LOGTAG, "onConfigurationChanged: " + newConfig.locale);

        if (lastLocale.equals(newConfig.locale)) {
            Log.d(LOGTAG, "Old locale same as new locale. Short-circuiting.");
            return;
        }

        final LocaleManager localeManager = BrowserLocaleManager.getInstance();
        final Locale changed = localeManager.onSystemConfigurationChanged(this, getResources(), newConfig, lastLocale);
        if (changed != null) {
            onLocaleChanged(changed);
        }
    }

    /**
     * Implementation for the {@link OnSharedPreferenceChangeListener} interface,
     * which we use to watch changes in our prefs file.
     *
     * This is reliably called whenever the pref changes, which is not the case
     * for multiple consecutive changes in the case of onPreferenceChange.
     *
     * Note that this listener is not always registered: we use it only on
     * tablets, Honeycomb and up, where we'll have a multi-pane view and prefs
     * changing multiple times.
     */
    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
        if (PREFS_BROWSER_LOCALE.equals(key)) {
            onLocaleSelected(Locales.getLanguageTag(lastLocale),
                             sharedPreferences.getString(key, null));
        }
    }

    public interface PrefHandler {
        // Allows the pref to do any initialization it needs. Return false to have the pref removed
        // from the prefs screen entirely.
        public boolean setupPref(Context context, Preference pref);
        public void onChange(Context context, Preference pref, Object newValue);
    }

    private void recordSettingChangeTelemetry(String prefName, Object newValue) {
        final String value;
        if (newValue instanceof Boolean) {
            value = (Boolean) newValue ? "1" : "0";
        } else if (prefName.equals(PREFS_HOMEPAGE)) {
            // Don't record the user's homepage preference.
            value = "*";
        } else {
            value = newValue.toString();
        }

        final JSONArray extras = new JSONArray();
        extras.put(prefName);
        extras.put(value);
        Telemetry.sendUIEvent(TelemetryContract.Event.EDIT, Method.SETTINGS, extras.toString());
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        final String prefName = preference.getKey();
        Log.i(LOGTAG, "Changed " + prefName + " = " + newValue);
        recordSettingChangeTelemetry(prefName, newValue);

        if (PREFS_MP_ENABLED.equals(prefName)) {
            showDialog((Boolean) newValue ? DIALOG_CREATE_MASTER_PASSWORD : DIALOG_REMOVE_MASTER_PASSWORD);

            // We don't want the "use master password" pref to change until the
            // user has gone through the dialog.
            return false;
        }

        if (PREFS_HOMEPAGE.equals(prefName)) {
            setHomePageSummary(preference, String.valueOf(newValue));
        }

        if (PREFS_BROWSER_LOCALE.equals(prefName)) {
            // Even though this is a list preference, we don't want to handle it
            // below, so we return here.
            return onLocaleSelected(Locales.getLanguageTag(lastLocale), (String) newValue);
        }

        if (PREFS_MENU_CHAR_ENCODING.equals(prefName)) {
            setCharEncodingState(((String) newValue).equals("true"));
        } else if (PREFS_UPDATER_AUTODOWNLOAD.equals(prefName)) {
            UpdateServiceHelper.setAutoDownloadPolicy(this, UpdateService.AutoDownloadPolicy.get((String) newValue));
        } else if (PREFS_UPDATER_URL.equals(prefName)) {
            UpdateServiceHelper.setUpdateUrl(this, (String) newValue);
        } else if (PREFS_HEALTHREPORT_UPLOAD_ENABLED.equals(prefName)) {
            final Boolean newBooleanValue = (Boolean) newValue;
            AdjustConstants.getAdjustHelper().setEnabled(newBooleanValue);
        } else if (PREFS_GEO_REPORTING.equals(prefName)) {
            if ((Boolean) newValue) {
                enableStumbler((CheckBoxPreference) preference);
                return false;
            } else {
                broadcastStumblerPref(GeckoPreferences.this, false);
                return true;
            }
        } else if (PREFS_TAB_QUEUE.equals(prefName)) {
            if ((Boolean) newValue && !TabQueueHelper.canDrawOverlays(this)) {
                Intent promptIntent = new Intent(this, TabQueuePrompt.class);
                startActivityForResult(promptIntent, REQUEST_CODE_TAB_QUEUE);
                return false;
            }
        } else if (PREFS_NOTIFICATIONS_CONTENT.equals(prefName)) {
            FeedService.setup(this);
        } else if (PREFS_ACTIVITY_STREAM.equals(prefName)) {
            ThreadUtils.postDelayedToUiThread(new Runnable() {
                @Override
                public void run() {
                    GeckoAppShell.scheduleRestart();
                }
            }, 1000);
        } else if (HANDLERS.containsKey(prefName)) {
            PrefHandler handler = HANDLERS.get(prefName);
            handler.onChange(this, preference, newValue);
        } else if (PREFS_SEARCH_SUGGESTIONS_ENABLED.equals(prefName)) {
            // Tell Gecko to transmit the current search engine data again, so
            // BrowserSearch is notified immediately about the new enabled state.
            EventDispatcher.getInstance().dispatch("SearchEngines:GetVisible", null);
        }

        // Send Gecko-side pref changes to Gecko
        if (isGeckoPref(prefName)) {
            PrefsHelper.setPref(prefName, newValue, true /* flush */);
        }

        if (preference instanceof ListPreference) {
            // We need to find the entry for the new value
            int newIndex = ((ListPreference) preference).findIndexOfValue((String) newValue);
            CharSequence newEntry = ((ListPreference) preference).getEntries()[newIndex];
            ((ListPreference) preference).setSummary(newEntry);
        } else if (preference instanceof LinkPreference) {
            setResult(RESULT_CODE_EXIT_SETTINGS);
            finishChoosingTransition();
        }

        return true;
    }

    private void enableStumbler(final CheckBoxPreference preference) {
        Permissions
                .from(this)
                .withPermissions(Manifest.permission.ACCESS_FINE_LOCATION)
                .onUIThread()
                .andFallback(new Runnable() {
                    @Override
                    public void run() {
                        preference.setChecked(false);
                    }
                })
                .run(new Runnable() {
                    @Override
                    public void run() {
                        preference.setChecked(true);
                        broadcastStumblerPref(GeckoPreferences.this, true);
                    }
                });
    }

    private TextInputLayout getTextBox(int aHintText) {
        final EditText input = new EditText(this);
        int inputtype = InputType.TYPE_CLASS_TEXT;
        inputtype |= InputType.TYPE_TEXT_VARIATION_PASSWORD | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
        input.setInputType(inputtype);

        input.setHint(aHintText);

        final TextInputLayout layout = new TextInputLayout(this);
        layout.addView(input);

        return layout;
    }

    private class PasswordTextWatcher implements TextWatcher {
        EditText input1;
        EditText input2;
        AlertDialog dialog;

        PasswordTextWatcher(EditText aInput1, EditText aInput2, AlertDialog aDialog) {
            input1 = aInput1;
            input2 = aInput2;
            dialog = aDialog;
        }

        @Override
        public void afterTextChanged(Editable s) {
            if (dialog == null)
                return;

            String text1 = input1.getText().toString();
            String text2 = input2.getText().toString();
            boolean disabled = TextUtils.isEmpty(text1) || TextUtils.isEmpty(text2) || !text1.equals(text2);
            dialog.getButton(DialogInterface.BUTTON_POSITIVE).setEnabled(!disabled);
        }

        @Override
        public void beforeTextChanged(CharSequence s, int start, int count, int after) { }
        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count) { }
    }

    private class EmptyTextWatcher implements TextWatcher {
        EditText input;
        AlertDialog dialog;

        EmptyTextWatcher(EditText aInput, AlertDialog aDialog) {
            input = aInput;
            dialog = aDialog;
        }

        @Override
        public void afterTextChanged(Editable s) {
            if (dialog == null)
                return;

            String text = input.getText().toString();
            boolean disabled = TextUtils.isEmpty(text);
            dialog.getButton(DialogInterface.BUTTON_POSITIVE).setEnabled(!disabled);
        }

        @Override
        public void beforeTextChanged(CharSequence s, int start, int count, int after) { }
        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count) { }
    }

    @Override
    protected Dialog onCreateDialog(int id) {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        LinearLayout linearLayout = new LinearLayout(this);
        linearLayout.setOrientation(LinearLayout.VERTICAL);
        AlertDialog dialog;
        switch (id) {
            case DIALOG_CREATE_MASTER_PASSWORD:
                final TextInputLayout inputLayout1 = getTextBox(R.string.masterpassword_password);
                final TextInputLayout inputLayout2 = getTextBox(R.string.masterpassword_confirm);
                linearLayout.addView(inputLayout1);
                linearLayout.addView(inputLayout2);

                final EditText input1 = inputLayout1.getEditText();
                final EditText input2 = inputLayout2.getEditText();

                builder.setTitle(R.string.masterpassword_create_title)
                       .setView((View) linearLayout)
                       .setPositiveButton(R.string.button_ok, new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                PrefsHelper.setPref(PREFS_MP_ENABLED,
                                                    input1.getText().toString(),
                                                    /* flush */ true);
                            }
                        })
                        .setNegativeButton(R.string.button_cancel, new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                return;
                            }
                        });
                        dialog = builder.create();
                        dialog.setOnShowListener(new DialogInterface.OnShowListener() {
                            @Override
                            public void onShow(DialogInterface dialog) {
                                input1.setText("");
                                input2.setText("");
                                input1.requestFocus();
                            }
                        });

                        PasswordTextWatcher watcher = new PasswordTextWatcher(input1, input2, dialog);
                        input1.addTextChangedListener((TextWatcher) watcher);
                        input2.addTextChangedListener((TextWatcher) watcher);

                break;
            case DIALOG_REMOVE_MASTER_PASSWORD:
                final TextInputLayout inputLayout = getTextBox(R.string.masterpassword_password);
                linearLayout.addView(inputLayout);
                final EditText input = inputLayout.getEditText();

                builder.setTitle(R.string.masterpassword_remove_title)
                       .setView((View) linearLayout)
                       .setPositiveButton(R.string.button_ok, new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                PrefsHelper.setPref(PREFS_MP_ENABLED, input.getText().toString());
                            }
                        })
                        .setNegativeButton(R.string.button_cancel, new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                return;
                            }
                        });
                        dialog = builder.create();
                        dialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
                            @Override
                            public void onDismiss(DialogInterface dialog) {
                                input.setText("");
                            }
                        });
                        dialog.setOnShowListener(new DialogInterface.OnShowListener() {
                            @Override
                            public void onShow(DialogInterface dialog) {
                                input.setText("");
                            }
                        });
                        input.addTextChangedListener(new EmptyTextWatcher(input, dialog));
                break;
            default:
                return null;
        }

        return dialog;
    }

    // Initialize preferences by requesting the preference values from Gecko
    private static class PrefCallbacks extends PrefsHelper.PrefHandlerBase {
        private final PreferenceGroup screen;

        public PrefCallbacks(final PreferenceGroup screen) {
            this.screen = screen;
        }

        private Preference getField(String prefName) {
            return screen.findPreference(prefName);
        }

        @Override
        public void prefValue(String prefName, final boolean value) {
            final TwoStatePreference pref = (TwoStatePreference) getField(prefName);
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    if (pref.isChecked() != value) {
                        pref.setChecked(value);
                    }
                }
            });
        }

        @Override
        public void prefValue(String prefName, final String value) {
            final Preference pref = getField(prefName);
            if (pref instanceof EditTextPreference) {
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        ((EditTextPreference) pref).setText(value);
                    }
                });
            } else if (pref instanceof ListPreference) {
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        ((ListPreference) pref).setValue(value);
                        // Set the summary string to the current entry
                        CharSequence selectedEntry = ((ListPreference) pref).getEntry();
                        ((ListPreference) pref).setSummary(selectedEntry);
                    }
                });
            }
        }

        @Override
        public void prefValue(String prefName, final int value) {
            final Preference pref = getField(prefName);
            Log.w(LOGTAG, "Unhandled int value for pref [" + pref + "]");
        }

        @Override
        public void finish() {
            // enable all preferences once we have them from gecko
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    screen.setEnabled(true);
                }
            });
        }
    }

    private PrefsHelper.PrefHandler getGeckoPreferences(final PreferenceGroup screen,
                                                            ArrayList<String> prefs) {
        final PrefsHelper.PrefHandler prefHandler = new PrefCallbacks(screen);
        final String[] prefNames = prefs.toArray(new String[prefs.size()]);
        PrefsHelper.addObserver(prefNames, prefHandler);
        return prefHandler;
    }

    @Override
    public boolean isGeckoActivityOpened() {
        return mGeckoActivityOpened;
    }

    /**
     * Given an Intent instance, add extras to specify which settings section to
     * open.
     *
     * resource should be a valid Android XML resource identifier.
     *
     * The mechanism to open a section differs based on Android version.
     */
    public static void setResourceToOpen(final Intent intent, final String resource) {
        if (intent == null) {
            throw new IllegalArgumentException("intent must not be null");
        }
        if (resource == null) {
            return;
        }

        intent.putExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT, GeckoPreferenceFragment.class.getName());

        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString("resource", resource);
        intent.putExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArgs);
    }
}
