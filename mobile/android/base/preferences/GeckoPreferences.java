/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

import android.os.Build;

import org.json.JSONObject;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.BrowserLocaleManager;
import org.mozilla.gecko.DataReportingNotification;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoActivityStatus;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoApplication;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.LocaleManager;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.R;
import org.mozilla.gecko.RestrictedProfiles;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.TelemetryContract.Method;
import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.healthreport.HealthReportConstants;
import org.mozilla.gecko.db.BrowserContract.SuggestedSites;
import org.mozilla.gecko.updater.UpdateService;
import org.mozilla.gecko.updater.UpdateServiceHelper;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.InputOptionsUtils;
import org.mozilla.gecko.widget.FloatingHintEditText;

import android.app.ActionBar;
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
import android.os.Bundle;
import android.preference.CheckBoxPreference;
import android.preference.EditTextPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceActivity;
import android.preference.PreferenceGroup;
import android.preference.PreferenceScreen;
import android.preference.TwoStatePreference;
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
import android.widget.Toast;

public class GeckoPreferences
extends PreferenceActivity
implements
GeckoActivityStatus,
GeckoEventListener,
OnPreferenceChangeListener,
OnSharedPreferenceChangeListener
{
    private static final String LOGTAG = "GeckoPreferences";

    // We have a white background, which makes transitions on
    // some devices look bad. Don't use transitions on those
    // devices.
    private static final boolean NO_TRANSITIONS = HardwareUtils.IS_KINDLE_DEVICE;

    public static final String NON_PREF_PREFIX = "android.not_a_preference.";
    public static final String INTENT_EXTRA_RESOURCES = "resource";
    public static String PREFS_HEALTHREPORT_UPLOAD_ENABLED = NON_PREF_PREFIX + "healthreport.uploadEnabled";

    private static boolean sIsCharEncodingEnabled;
    private boolean mInitialized;
    private int mPrefsRequestId;
    private PanelsPreferenceCategory mPanelsPreferenceCategory;

    // These match keys in resources/xml*/preferences*.xml
    private static final String PREFS_SEARCH_RESTORE_DEFAULTS = NON_PREF_PREFIX + "search.restore_defaults";
    private static final String PREFS_DATA_REPORTING_PREFERENCES = NON_PREF_PREFIX + "datareporting.preferences";
    private static final String PREFS_TELEMETRY_ENABLED = "toolkit.telemetry.enabled";
    private static final String PREFS_CRASHREPORTER_ENABLED = "datareporting.crashreporter.submitEnabled";
    private static final String PREFS_MENU_CHAR_ENCODING = "browser.menu.showCharacterEncoding";
    private static final String PREFS_MP_ENABLED = "privacy.masterpassword.enabled";
    private static final String PREFS_LOGIN_MANAGE = NON_PREF_PREFIX + "signon.manage";
    private static final String PREFS_UPDATER_AUTODOWNLOAD = "app.update.autodownload";
    private static final String PREFS_UPDATER_URL = "app.update.url.android";
    private static final String PREFS_GEO_REPORTING = NON_PREF_PREFIX + "app.geo.reportdata";
    private static final String PREFS_GEO_LEARN_MORE = NON_PREF_PREFIX + "geo.learn_more";
    private static final String PREFS_HEALTHREPORT_LINK = NON_PREF_PREFIX + "healthreport.link";
    private static final String PREFS_DEVTOOLS_REMOTE_USB_ENABLED = "devtools.remote.usb.enabled";
    private static final String PREFS_DEVTOOLS_REMOTE_WIFI_ENABLED = "devtools.remote.wifi.enabled";
    private static final String PREFS_DISPLAY_REFLOW_ON_ZOOM = "browser.zoom.reflowOnZoom";
    private static final String PREFS_DISPLAY_TITLEBAR_MODE = "browser.chrome.titlebarMode";
    private static final String PREFS_SYNC = NON_PREF_PREFIX + "sync";
    public static final String PREFS_OPEN_URLS_IN_PRIVATE = NON_PREF_PREFIX + "openExternalURLsPrivately";
    public static final String PREFS_VOICE_INPUT_ENABLED = NON_PREF_PREFIX + "voice_input_enabled";
    public static final String PREFS_QRCODE_ENABLED = NON_PREF_PREFIX + "qrcode_enabled";
    private static final String PREFS_DEVTOOLS = NON_PREF_PREFIX + "devtools.enabled";

    private static final String ACTION_STUMBLER_UPLOAD_PREF = AppConstants.ANDROID_PACKAGE_NAME + ".STUMBLER_PREF";


    // This isn't a Gecko pref, even if it looks like one.
    private static final String PREFS_BROWSER_LOCALE = "locale";

    public static final String PREFS_RESTORE_SESSION = NON_PREF_PREFIX + "restoreSession3";
    public static final String PREFS_SUGGESTED_SITES = NON_PREF_PREFIX + "home_suggested_sites";
    public static final String PREFS_TAB_QUEUE = NON_PREF_PREFIX + "tab_queue";
    public static final String PREFS_CUSTOMIZE_SCREEN = NON_PREF_PREFIX + "customize_screen";
    public static final String PREFS_TAB_QUEUE_LAST_SITE = NON_PREF_PREFIX + "last_site";
    public static final String PREFS_TAB_QUEUE_LAST_TIME = NON_PREF_PREFIX + "last_time";

    // These values are chosen to be distinct from other Activity constants.
    private static final int REQUEST_CODE_PREF_SCREEN = 5;
    private static final int RESULT_CODE_EXIT_SETTINGS = 6;

    // Result code used when a locale preference changes.
    // Callers can recognize this code to refresh themselves to
    // accommodate a locale change.
    public static final int RESULT_CODE_LOCALE_DID_CHANGE = 7;

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
        if (Versions.feature14Plus) {
            final String newTitle = getString(title);
            if (newTitle != null) {
                Log.v(LOGTAG, "Setting action bar title to " + newTitle);

                final ActionBar actionBar = getActionBar();
                if (actionBar != null) {
                    actionBar.setTitle(newTitle);
                }
            }
        }
    }

    private void updateTitle(String newTitle) {
        if (newTitle != null) {
            Log.v(LOGTAG, "Setting activity title to " + newTitle);
            setTitle(newTitle);
        }
    }

    private void updateTitle(int title) {
        updateTitle(getString(title));
    }

    /**
     * This updates the title shown above the prefs fragment in
     * a multi-pane view.
     */
    private void updateBreadcrumbTitle(int title) {
        final String newTitle = getString(title);
        showBreadCrumbs(newTitle, newTitle);
    }

    private void updateTitleForPrefsResource(int res) {
        // If we're a multi-pane view, the activity title is really
        // the header bar above the fragment.
        // Find out which fragment we're showing, and use that.
        if (Versions.feature11Plus && isMultiPane()) {
            int title = getIntent().getIntExtra(EXTRA_SHOW_FRAGMENT_TITLE, -1);
            if (res == R.xml.preferences) {
                // This should only occur when res == R.xml.preferences,
                // but showing "Settings" is better than crashing or showing
                // "Fennec".
                updateActionBarTitle(R.string.settings_title);
            }

            updateTitle(title);
            updateBreadcrumbTitle(title);
            return;
        }

        // At present we only need to do this for non-leaf prefs views
        // and the locale switcher itself.
        int title = -1;
        if (res == R.xml.preferences) {
            title = R.string.settings_title;
        } else if (res == R.xml.preferences_locale) {
            title = R.string.pref_category_language;
        } else if (res == R.xml.preferences_vendor) {
            title = R.string.pref_category_vendor;
        } else if (res == R.xml.preferences_customize) {
            title = R.string.pref_category_customize;
        }
        if (title != -1) {
            updateTitle(title);
        }
    }

    private void onLocaleChanged(Locale newLocale) {
        Log.d(LOGTAG, "onLocaleChanged: " + newLocale);

        BrowserLocaleManager.getInstance().updateConfiguration(getApplicationContext(), newLocale);
        this.lastLocale = newLocale;

        if (Versions.feature11Plus && isMultiPane()) {
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

            updateTitle(R.string.pref_header_language);
            updateBreadcrumbTitle(R.string.pref_header_language);

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
        final Locale currentLocale = Locale.getDefault();
        Log.v(LOGTAG, "Checking locale: " + currentLocale + " vs " + lastLocale);
        if (currentLocale.equals(lastLocale)) {
            return;
        }

        onLocaleChanged(currentLocale);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Make sure RestrictedProfiles is ready.
        RestrictedProfiles.initWithProfile(GeckoProfile.get(this));

        // Apply the current user-selected locale, if necessary.
        checkLocale();

        // Track this so we can decide whether to show locale options.
        // See also the workaround below for Bug 1015209.
        localeSwitchingIsEnabled = BrowserLocaleManager.getInstance().isEnabled();

        // For Android v11+ where we use Fragments (v11+ only due to bug 866352),
        // check that PreferenceActivity.EXTRA_SHOW_FRAGMENT has been set
        // (or set it) before super.onCreate() is called so Android can display
        // the correct Fragment resource.

        if (Versions.feature11Plus) {
            if (!getIntent().hasExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT)) {
                // Set up the default fragment if there is no explicit fragment to show.
                setupTopLevelFragmentIntent();

                // This is the default header, because it's the first one.
                // I know, this is an affront to all human decency. And yet.
                updateTitle(getString(R.string.pref_header_customize));
            }

            if (onIsMultiPane()) {
                // So that Android doesn't put the fragment title (or nothing at
                // all) in the action bar.
                updateActionBarTitle(R.string.settings_title);

                if (Build.VERSION.SDK_INT < 13) {
                    // Affected by Bug 1015209 -- no detach/attach.
                    // If we try rejigging fragments, we'll crash, so don't
                    // enable locale switching at all.
                    localeSwitchingIsEnabled = false;
                }
            }
        }

        super.onCreate(savedInstanceState);
        initActionBar();

        // Use setResourceToOpen to specify these extras.
        Bundle intentExtras = getIntent().getExtras();

        // For versions of Android lower than Honeycomb, use xml resources instead of
        // Fragments because of an Android bug in ActionBar (described in bug 866352 and
        // fixed in bug 833625).
        if (Versions.preHC) {
            // Write prefs to our custom GeckoSharedPrefs file.
            getPreferenceManager().setSharedPreferencesName(GeckoSharedPrefs.APP_PREFS_NAME);

            int res = 0;
            if (intentExtras != null && intentExtras.containsKey(INTENT_EXTRA_RESOURCES)) {
                // Fetch resource id from intent.
                final String resourceName = intentExtras.getString(INTENT_EXTRA_RESOURCES);
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, Method.SETTINGS, resourceName);

                if (resourceName != null) {
                    res = getResources().getIdentifier(resourceName, "xml", getPackageName());
                    if (res == 0) {
                        Log.e(LOGTAG, "No resource found named " + resourceName);
                    }
                }
            }
            if (res == 0) {
                // No resource specified, or the resource was invalid; use the default preferences screen.
                Log.e(LOGTAG, "Displaying default settings.");
                res = R.xml.preferences;
                Telemetry.startUISession(TelemetryContract.Session.SETTINGS);
            }

            // We don't include a title in the XML, so set it here, in a locale-aware fashion.
            updateTitleForPrefsResource(res);
            addPreferencesFromResource(res);
        }

        EventDispatcher.getInstance().registerGeckoThreadListener(this,
            "Sanitize:Finished");

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
            NotificationManager notificationManager = (NotificationManager) this.getSystemService(Context.NOTIFICATION_SERVICE);
            notificationManager.cancel(DataReportingNotification.ALERT_NAME_DATAREPORTING_NOTIFICATION.hashCode());
        }
    }

    /**
     * Initializes the action bar configuration in code.
     *
     * Declaring these attributes in XML does not work on some devices for an unknown reason
     * (e.g. the back button stops working or the logo disappears; see bug 1152314) so we
     * duplicate those attributes in code here. Note: the order of these calls matters.
     *
     * We keep the XML attributes because not all of these methods are available on pre-v14.
     */
    private void initActionBar() {
        if (Versions.feature14Plus) {
            final ActionBar actionBar = getActionBar();
            if (actionBar != null) {
                actionBar.setHomeButtonEnabled(true);
                actionBar.setDisplayHomeAsUpEnabled(true);
                actionBar.setLogo(R.drawable.logo);
                actionBar.setDisplayUseLogoEnabled(true);
            }
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
                fragmentArgs.putString(INTENT_EXTRA_RESOURCES, "preferences_customize_tablet");
            }
        }

        // Build fragment intent.
        intent.putExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT, GeckoPreferenceFragment.class.getName());
        intent.putExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArgs);
    }

    @Override
    public boolean isValidFragment(String fragmentName) {
        return GeckoPreferenceFragment.class.getName().equals(fragmentName);
    }

    @Override
    public void onBuildHeaders(List<Header> target) {
        if (onIsMultiPane()) {
            loadHeadersFromResource(R.xml.preference_headers, target);

            // If locale switching is disabled, remove the section
            // entirely. This logic will need to be extended when
            // content language selection (Bug 881510) is implemented.
            if (!localeSwitchingIsEnabled) {
                for (Header header : target) {
                    if (header.id == R.id.pref_header_language) {
                        target.remove(header);
                        break;
                    }
                }
            }
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        if (!hasFocus || mInitialized)
            return;

        mInitialized = true;
        if (Versions.preHC) {
            PreferenceScreen screen = getPreferenceScreen();
            mPrefsRequestId = setupPreferences(screen);
        }
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
        EventDispatcher.getInstance().unregisterGeckoThreadListener(this,
            "Sanitize:Finished");
        if (mPrefsRequestId > 0) {
            PrefsHelper.removeObserver(mPrefsRequestId);
        }

        // The intent extras will be null if this is the top-level settings
        // activity. In that case, we want to end the SETTINGS telmetry session.
        // For HC+ versions of Android this is handled in GeckoPreferenceFragment.
        if (Versions.preHC && getIntent().getExtras() == null) {
            Telemetry.stopUISession(TelemetryContract.Session.SETTINGS);
        }
    }

    @Override
    public void onPause() {
        // Symmetric with onResume.
        if (Versions.feature11Plus) {
            if (isMultiPane()) {
                SharedPreferences prefs = GeckoSharedPrefs.forApp(this);
                prefs.unregisterOnSharedPreferenceChangeListener(this);
            }
        }

        super.onPause();

        if (getApplication() instanceof GeckoApplication) {
            ((GeckoApplication) getApplication()).onActivityPause(this);
        }
    }

    @Override
    public void onResume() {
        super.onResume();

        if (getApplication() instanceof GeckoApplication) {
            ((GeckoApplication) getApplication()).onActivityResume(this);
        }

        if (Versions.feature11Plus) {
            // Watch prefs, otherwise we don't reliably get told when they change.
            // See documentation for onSharedPreferenceChange for more.
            // Inexplicably only needed on tablet.
            if (isMultiPane()) {
                SharedPreferences prefs = GeckoSharedPrefs.forApp(this);
                prefs.registerOnSharedPreferenceChangeListener(this);
            }
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
    public void startWithFragment(String fragmentName, Bundle args,
            Fragment resultTo, int resultRequestCode, int titleRes, int shortTitleRes) {
        Log.v(LOGTAG, "Starting with fragment: " + fragmentName + ", title " + titleRes);

        // Overriding because we want to use startActivityForResult for Fragment intents.
        Intent intent = onBuildStartFragmentIntent(fragmentName, args, titleRes, shortTitleRes);
        if (resultTo == null) {
            startActivityForResultChoosingTransition(intent, REQUEST_CODE_PREF_SCREEN);
        } else {
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
        }
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        try {
            if (event.equals("Sanitize:Finished")) {
                boolean success = message.getBoolean("success");
                final int stringRes = success ? R.string.private_data_success : R.string.private_data_fail;
                final Context context = this;
                ThreadUtils.postToUiThread(new Runnable () {
                    @Override
                    public void run() {
                        Toast.makeText(context, stringRes, Toast.LENGTH_SHORT).show();
                    }
                });
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    /**
      * Initialize all of the preferences (native of Gecko ones) for this screen.
      *
      * @param prefs The android.preference.PreferenceGroup to initialize
      * @return The integer id for the PrefsHelper.PrefHandlerBase listener added
      *         to monitor changes to Gecko prefs.
      */
    public int setupPreferences(PreferenceGroup prefs) {
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
            Preference pref = preferences.getPreference(i);

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
                    if (!AppConstants.MOZ_DATA_REPORTING) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (pref instanceof PanelsPreferenceCategory) {
                    mPanelsPreferenceCategory = (PanelsPreferenceCategory) pref;
                }
                if((AppConstants.MOZ_ANDROID_TAB_QUEUE && AppConstants.NIGHTLY_BUILD) && (PREFS_CUSTOMIZE_SCREEN.equals(key))) {
                    // Only change the customize pref screen summary on nightly builds with the tab queue build flag.
                    pref.setSummary(getString(R.string.pref_category_customize_alt_summary));
                }
                if (getResources().getString(R.string.pref_category_input_options).equals(key)) {
                    if (!AppConstants.NIGHTLY_BUILD || (!InputOptionsUtils.supportsVoiceRecognizer(getApplicationContext(), getResources().getString(R.string.voicesearch_prompt)) &&
                            !InputOptionsUtils.supportsQrCodeReader(getApplicationContext()))) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                }
                if (PREFS_DEVTOOLS.equals(key) &&
                    RestrictedProfiles.isUserRestricted()) {
                    preferences.removePreference(pref);
                    i--;
                    continue;
                }

                setupPreferences((PreferenceGroup) pref, prefs);
            } else {
                pref.setOnPreferenceChangeListener(this);
                if (PREFS_UPDATER_AUTODOWNLOAD.equals(key)) {
                    if (!AppConstants.MOZ_UPDATER) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_LOGIN_MANAGE.equals(key)) {
                    if (!AppConstants.NIGHTLY_BUILD) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_DISPLAY_REFLOW_ON_ZOOM.equals(key)) {
                    // Remove UI for reflow on release builds.
                    if (AppConstants.RELEASE_BUILD) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_OPEN_URLS_IN_PRIVATE.equals(key)) {
                    // Remove UI for opening external links in private browsing on non-Nightly builds.
                    if (!AppConstants.NIGHTLY_BUILD) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_TELEMETRY_ENABLED.equals(key)) {
                    if (!AppConstants.MOZ_TELEMETRY_REPORTING) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_HEALTHREPORT_UPLOAD_ENABLED.equals(key) ||
                           PREFS_HEALTHREPORT_LINK.equals(key)) {
                    if (!AppConstants.MOZ_SERVICES_HEALTHREPORT) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_CRASHREPORTER_ENABLED.equals(key)) {
                    if (!AppConstants.MOZ_CRASHREPORTER) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_GEO_REPORTING.equals(key) ||
                           PREFS_GEO_LEARN_MORE.equals(key)) {
                    if (!AppConstants.MOZ_STUMBLER_BUILD_TIME_ENABLED) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_DEVTOOLS_REMOTE_USB_ENABLED.equals(key)) {
                    if (!RestrictedProfiles.isAllowed(this, RestrictedProfiles.Restriction.DISALLOW_REMOTE_DEBUGGING)) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_DEVTOOLS_REMOTE_WIFI_ENABLED.equals(key)) {
                    if (!RestrictedProfiles.isAllowed(this, RestrictedProfiles.Restriction.DISALLOW_REMOTE_DEBUGGING)) {
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
                    if (!RestrictedProfiles.isAllowed(this, RestrictedProfiles.Restriction.DISALLOW_MODIFY_ACCOUNTS)) {
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
                } else if (PREFS_DISPLAY_TITLEBAR_MODE.equals(key)) {
                    if (HardwareUtils.isTablet()) {
                        // New tablet always shows URLS, not titles.
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (handlers.containsKey(key)) {
                    PrefHandler handler = handlers.get(key);
                    if (!handler.setupPref(this, pref)) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_TAB_QUEUE.equals(key)) {
                    // Only show tab queue pref on nightly builds with the tab queue build flag.
                    if (!(AppConstants.MOZ_ANDROID_TAB_QUEUE && AppConstants.NIGHTLY_BUILD)) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_VOICE_INPUT_ENABLED.equals(key)) {
                    if (!AppConstants.NIGHTLY_BUILD || !InputOptionsUtils.supportsVoiceRecognizer(getApplicationContext(), getResources().getString(R.string.voicesearch_prompt))) {
                        // Remove UI for voice input on non nightly builds.
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                } else if (PREFS_QRCODE_ENABLED.equals(key)) {
                    if (!AppConstants.NIGHTLY_BUILD || !InputOptionsUtils.supportsQrCodeReader(getApplicationContext())) {
                        // Remove UI for qr code input on non nightly builds
                        preferences.removePreference(pref);
                        i--;
                        continue;
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
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("SearchEngines:RestoreDefaults", null));

        // Send message to Gecko to get engines. SearchPreferenceCategory listens for the response.
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("SearchEngines:GetVisible", null));
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
        context.sendBroadcast(intent, GlobalConstants.PER_ANDROID_PACKAGE_PERMISSION);
    }

    /**
     * Broadcast an intent with <code>pref</code>, <code>branch</code>, and
     * <code>enabled</code> extras. This is intended to represent the
     * notification of a preference value to observers.
     *
     * The broadcast will be sent only to receivers registered with the
     * (Fennec-specific) per-Android package permission.
     */
    public static void broadcastPrefAction(final Context context,
                                           final String action,
                                           final String pref,
                                           final boolean value) {
        final Intent intent = new Intent(action)
                .putExtra("pref", pref)
                .putExtra("branch", GeckoSharedPrefs.APP_PREFS_NAME)
                .putExtra("enabled", value);
        broadcastAction(context, intent);
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
     * <code>PREFS_HEALTHREPORT_UPLOAD_ENABLED</code> pref.
     */
    public static void broadcastHealthReportUploadPref(final Context context, final boolean value) {
        broadcastPrefAction(context,
                            HealthReportConstants.ACTION_HEALTHREPORT_UPLOAD_PREF,
                            PREFS_HEALTHREPORT_UPLOAD_ENABLED,
                            value);
    }

    /**
     * Broadcast the current value of the
     * <code>PREFS_HEALTHREPORT_UPLOAD_ENABLED</code> pref.
     */
    public static void broadcastHealthReportUploadPref(final Context context) {
        final boolean value = getBooleanPref(context, PREFS_HEALTHREPORT_UPLOAD_ENABLED, true);
        broadcastHealthReportUploadPref(context, value);
    }

    public static void broadcastHealthReportPrune(final Context context) {
        final Intent intent = new Intent(HealthReportConstants.ACTION_HEALTHREPORT_PRUNE);
        broadcastAction(context, intent);
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
        } else if (PREFS_SUGGESTED_SITES.equals(key)) {
            refreshSuggestedSites();
        }
    }

    public interface PrefHandler {
        // Allows the pref to do any initialization it needs. Return false to have the pref removed
        // from the prefs screen entirely.
        public boolean setupPref(Context context, Preference pref);
        public void onChange(Context context, Preference pref, Object newValue);
    }

    @SuppressWarnings("serial")
    private final Map<String, PrefHandler> handlers = new HashMap<String, PrefHandler>() {{
        put(ClearOnShutdownPref.PREF, new ClearOnShutdownPref());
        put(AndroidImportPreference.PREF_KEY, new AndroidImportPreference.Handler());
    }};

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        final String prefName = preference.getKey();
        Log.i(LOGTAG, "Changed " + prefName + " = " + newValue);

        Telemetry.sendUIEvent(TelemetryContract.Event.EDIT, Method.SETTINGS, prefName);

        if (PREFS_MP_ENABLED.equals(prefName)) {
            showDialog((Boolean) newValue ? DIALOG_CREATE_MASTER_PASSWORD : DIALOG_REMOVE_MASTER_PASSWORD);

            // We don't want the "use master password" pref to change until the
            // user has gone through the dialog.
            return false;
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
            // The healthreport pref only lives in Android, so we do not persist
            // to Gecko, but we do broadcast intent to the health report
            // background uploader service, which will start or stop the
            // repeated background upload attempts.
            broadcastHealthReportUploadPref(this, (Boolean) newValue);
        } else if (PREFS_GEO_REPORTING.equals(prefName)) {
            broadcastStumblerPref(this, (Boolean) newValue);
            // Translate boolean value to int for geo reporting pref.
            newValue = (Boolean) newValue ? 1 : 0;
        } else if (handlers.containsKey(prefName)) {
            PrefHandler handler = handlers.get(prefName);
            handler.onChange(this, preference, newValue);
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
        } else if (preference instanceof FontSizePreference) {
            final FontSizePreference fontSizePref = (FontSizePreference) preference;
            fontSizePref.setSummary(fontSizePref.getSavedFontSizeName());
        }

        return true;
    }

    private EditText getTextBox(int aHintText) {
        EditText input = new FloatingHintEditText(this);
        int inputtype = InputType.TYPE_CLASS_TEXT;
        inputtype |= InputType.TYPE_TEXT_VARIATION_PASSWORD | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
        input.setInputType(inputtype);

        input.setHint(aHintText);
        return input;
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
        switch(id) {
            case DIALOG_CREATE_MASTER_PASSWORD:
                final EditText input1 = getTextBox(R.string.masterpassword_password);
                final EditText input2 = getTextBox(R.string.masterpassword_confirm);
                linearLayout.addView(input1);
                linearLayout.addView(input2);

                builder.setTitle(R.string.masterpassword_create_title)
                       .setView((View) linearLayout)
                       .setPositiveButton(R.string.button_ok, new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                JSONObject jsonPref = new JSONObject();
                                try {
                                    jsonPref.put("name", PREFS_MP_ENABLED);
                                    jsonPref.put("flush", true);
                                    jsonPref.put("type", "string");
                                    jsonPref.put("value", input1.getText().toString());

                                    GeckoEvent event = GeckoEvent.createBroadcastEvent("Preferences:Set", jsonPref.toString());
                                    GeckoAppShell.sendEventToGecko(event);
                                } catch(Exception ex) {
                                    Log.e(LOGTAG, "Error setting master password", ex);
                                }
                                return;
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
                final EditText input = getTextBox(R.string.masterpassword_password);
                linearLayout.addView(input);

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
    private int getGeckoPreferences(final PreferenceGroup screen, ArrayList<String> prefs) {
        return PrefsHelper.getPrefs(prefs, new PrefsHelper.PrefHandlerBase() {
            private Preference getField(String prefName) {
                return screen.findPreference(prefName);
            }

            // Handle v14 TwoStatePreference with backwards compatibility.
            class CheckBoxPrefSetter {
                public void setBooleanPref(Preference preference, boolean value) {
                    if ((preference instanceof CheckBoxPreference) &&
                       ((CheckBoxPreference) preference).isChecked() != value) {
                        ((CheckBoxPreference) preference).setChecked(value);
                    }
                }
            }

            class TwoStatePrefSetter extends CheckBoxPrefSetter {
                @Override
                public void setBooleanPref(Preference preference, boolean value) {
                    if ((preference instanceof TwoStatePreference) &&
                       ((TwoStatePreference) preference).isChecked() != value) {
                        ((TwoStatePreference) preference).setChecked(value);
                    }
                }
            }

            @Override
            public void prefValue(String prefName, final boolean value) {
                final Preference pref = getField(prefName);
                final CheckBoxPrefSetter prefSetter;
                if (Versions.preICS) {
                    prefSetter = new CheckBoxPrefSetter();
                } else {
                    prefSetter = new TwoStatePrefSetter();
                }
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        prefSetter.setBooleanPref(pref, value);
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
                } else if (pref instanceof FontSizePreference) {
                    final FontSizePreference fontSizePref = (FontSizePreference) pref;
                    fontSizePref.setSavedFontSize(value);
                    final String fontSizeName = fontSizePref.getSavedFontSizeName();
                    ThreadUtils.postToUiThread(new Runnable() {
                        @Override
                        public void run() {
                            fontSizePref.setSummary(fontSizeName); // Ex: "Small".
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
            public boolean isObserver() {
                return true;
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
        });
    }

    @Override
    public boolean isGeckoActivityOpened() {
        return false;
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

        if (Versions.preHC) {
            intent.putExtra("resource", resource);
        } else {
            intent.putExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT, GeckoPreferenceFragment.class.getName());

            Bundle fragmentArgs = new Bundle();
            fragmentArgs.putString("resource", resource);
            intent.putExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArgs);
        }
    }
}
