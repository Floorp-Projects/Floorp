/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.background.announcements.AnnouncementsConstants;
import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.healthreport.HealthReportConstants;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.GeckoPreferenceFragment;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONArray;
import org.json.JSONObject;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.Fragment;
import android.app.NotificationManager;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
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
import android.preference.PreferenceManager;
import android.preference.PreferenceScreen;
import android.preference.TwoStatePreference;
import android.text.Editable;
import android.text.InputType;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.Log;
import android.view.MenuItem;
import android.view.View;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.List;

public class GeckoPreferences
    extends PreferenceActivity
    implements OnPreferenceChangeListener, GeckoEventListener, GeckoActivityStatus
{
    private static final String LOGTAG = "GeckoPreferences";

    private static final String NON_PREF_PREFIX = "android.not_a_preference.";
    public static final String INTENT_EXTRA_RESOURCES = "resource";
    public static String PREFS_HEALTHREPORT_UPLOAD_ENABLED = NON_PREF_PREFIX + "healthreport.uploadEnabled";

    private static boolean sIsCharEncodingEnabled = false;
    private boolean mInitialized = false;
    private int mPrefsRequestId = 0;

    // These match keys in resources/xml/preferences.xml.in.
    private static String PREFS_ANNOUNCEMENTS_ENABLED = NON_PREF_PREFIX + "privacy.announcements.enabled";
    private static String PREFS_DATA_REPORTING_PREFERENCES = NON_PREF_PREFIX + "datareporting.preferences";
    private static String PREFS_TELEMETRY_ENABLED = "datareporting.telemetry.enabled";
    private static String PREFS_CRASHREPORTER_ENABLED = "datareporting.crashreporter.submitEnabled";
    private static String PREFS_MENU_CHAR_ENCODING = "browser.menu.showCharacterEncoding";
    private static String PREFS_MP_ENABLED = "privacy.masterpassword.enabled";
    private static String PREFS_UPDATER_AUTODOWNLOAD = "app.update.autodownload";
    private static String PREFS_GEO_REPORTING = "app.geo.reportdata";
    private static String PREFS_HEALTHREPORT_LINK = NON_PREF_PREFIX + "healthreport.link";
    private static String PREFS_DEVTOOLS_REMOTE_ENABLED = "devtools.debugger.remote-enabled";

    // These values are chosen to be distinct from other Activity constants.
    private static int REQUEST_CODE_PREF_SCREEN = 5;
    private static int RESULT_CODE_EXIT_SETTINGS = 6;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // For fragment-capable devices, display the default fragment if no explicit fragment to show.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB &&
            !getIntent().hasExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT)) {
            setupTopLevelFragmentIntent();
        }

        super.onCreate(savedInstanceState);

        // Use setResourceToOpen to specify these extras.
        Bundle intentExtras = getIntent().getExtras();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.HONEYCOMB) {
            int res = 0;
            if (intentExtras != null && intentExtras.containsKey(INTENT_EXTRA_RESOURCES)) {
                // Fetch resource id from intent.
                String resourceName = intentExtras.getString(INTENT_EXTRA_RESOURCES);
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
            }
            addPreferencesFromResource(res);
        }

        registerEventListener("Sanitize:Finished");

        if (Build.VERSION.SDK_INT >= 14)
            getActionBar().setHomeButtonEnabled(true);

        // If launched from notification, explicitly cancel the notification.
        if (intentExtras != null && intentExtras.containsKey(DataReportingNotification.ALERT_NAME_DATAREPORTING_NOTIFICATION)) {
            NotificationManager notificationManager = (NotificationManager) this.getSystemService(Context.NOTIFICATION_SERVICE);
            notificationManager.cancel(DataReportingNotification.ALERT_NAME_DATAREPORTING_NOTIFICATION.hashCode());
        }
    }

    /**
     * Set intent to display top-level settings fragment.
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
    public void onBuildHeaders(List<Header> target) {
        if (onIsMultiPane())
            loadHeadersFromResource(R.xml.preference_headers, target);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        if (!hasFocus || mInitialized)
            return;

        mInitialized = true;
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.HONEYCOMB) {
            PreferenceScreen screen = getPreferenceScreen();
            mPrefsRequestId = setupPreferences(screen);
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        unregisterEventListener("Sanitize:Finished");
        if (mPrefsRequestId > 0) {
            PrefsHelper.removeObserver(mPrefsRequestId);
        }
    }

    @Override
    public void onPause() {
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
    }

    @Override
    public void startActivity(Intent intent) {
        // For settings, we want to be able to pass results up the chain
        // of preference screens so Settings can behave as a single unit.
        // Specifically, when we open a link, we want to back out of all
        // the settings screens.
        // We need to start nested PreferenceScreens withStartActivityForResult().
        // Android doesn't let us do that (see Preference.onClick), so we're overriding here.
        startActivityForResult(intent, REQUEST_CODE_PREF_SCREEN);
    }

    @Override
    public void startWithFragment(String fragmentName, Bundle args,
            Fragment resultTo, int resultRequestCode, int titleRes, int shortTitleRes) {
        // Overriding because we want to use startActivityForResult for Fragment intents.
        Intent intent = onBuildStartFragmentIntent(fragmentName, args, titleRes, shortTitleRes);
        if (resultTo == null) {
            startActivityForResult(intent, REQUEST_CODE_PREF_SCREEN);
        } else {
            resultTo.startActivityForResult(intent, resultRequestCode);
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (REQUEST_CODE_PREF_SCREEN == requestCode && RESULT_CODE_EXIT_SETTINGS == resultCode) {
            // Pass this result up to the parent activity.
            setResult(RESULT_CODE_EXIT_SETTINGS);
            finish();
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
            String key = pref.getKey();
            if (pref instanceof PreferenceGroup) {
                // If no datareporting is enabled, remove UI.
                if (PREFS_DATA_REPORTING_PREFERENCES.equals(key)) {
                    if (!AppConstants.MOZ_DATA_REPORTING) {
                        preferences.removePreference(pref);
                        i--;
                        continue;
                    }
                }
                setupPreferences((PreferenceGroup) pref, prefs);
            } else {
                pref.setOnPreferenceChangeListener(this);
                if (!AppConstants.MOZ_UPDATER &&
                    PREFS_UPDATER_AUTODOWNLOAD.equals(key)) {
                    preferences.removePreference(pref);
                    i--;
                    continue;
                } else if (!AppConstants.MOZ_TELEMETRY_REPORTING &&
                           PREFS_TELEMETRY_ENABLED.equals(key)) {
                    preferences.removePreference(pref);
                    i--;
                    continue;
                } else if (!AppConstants.MOZ_SERVICES_HEALTHREPORT &&
                           (PREFS_HEALTHREPORT_UPLOAD_ENABLED.equals(key) ||
                            PREFS_HEALTHREPORT_LINK.equals(key))) {
                    preferences.removePreference(pref);
                    i--;
                    continue;
                } else if (!AppConstants.MOZ_CRASHREPORTER &&
                           PREFS_CRASHREPORTER_ENABLED.equals(key)) {
                    preferences.removePreference(pref);
                    i--;
                    continue;
                } else if (PREFS_GEO_REPORTING.equals(key) &&
                           ("release".equals(AppConstants.MOZ_UPDATE_CHANNEL) ||
                            "beta".equals(AppConstants.MOZ_UPDATE_CHANNEL))) {
                    // We don't build wifi/cell tower collection in release builds, so hide the UI.
                    preferences.removePreference(pref);
                    i--;
                    continue;
                } else if (PREFS_DEVTOOLS_REMOTE_ENABLED.equals(key)) {
                    final Context thisContext = this;
                    pref.setOnPreferenceClickListener(new OnPreferenceClickListener() {
                        @Override
                        public boolean onPreferenceClick(Preference preference) {
                            // Display toast to remind setting up tcp forwarding.
                            if (((CheckBoxPreference) preference).isChecked()) {
                                Toast.makeText(thisContext, R.string.devtools_remote_debugging_forward, Toast.LENGTH_SHORT).show();
                            }
                            return true;
                        }
                    });
                }

                // Some Preference UI elements are not actually preferences,
                // but they require a key to work correctly. For example,
                // "Clear private data" requires a key for its state to be
                // saved when the orientation changes. It uses the
                // "android.not_a_preference.privacy.clear" key - which doesn't
                // exist in Gecko - to satisfy this requirement.
                if (key != null && !key.startsWith(NON_PREF_PREFIX)) {
                    prefs.add(key);
                }
            }
        }
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home:
                finish();
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
        final Intent intent = new Intent(action);
        intent.setAction(action);
        intent.putExtra("pref", pref);
        intent.putExtra("branch", GeckoApp.PREFS_NAME);
        intent.putExtra("enabled", value);

        // There is a race here, but GeckoProfile returns the default profile
        // when Gecko is not explicitly running for a different profile.  In a
        // multi-profile world, this will need to be updated (possibly to
        // broadcast settings for all profiles).  See Bug 882182.
        GeckoProfile profile = GeckoProfile.get(context);
        if (profile != null) {
            intent.putExtra("profileName", profile.getName());
            intent.putExtra("profilePath", profile.getDir().getAbsolutePath());
        }

        Log.d(LOGTAG, "Broadcast: " + action + ", " + pref + ", " + GeckoApp.PREFS_NAME + ", " + value);
        context.sendBroadcast(intent, GlobalConstants.PER_ANDROID_PACKAGE_PERMISSION);
    }

    /**
     * Broadcast the provided value as the value of the
     * <code>PREFS_ANNOUNCEMENTS_ENABLED</code> pref.
     */
    public static void broadcastAnnouncementsPref(final Context context, final boolean value) {
        broadcastPrefAction(context,
                            AnnouncementsConstants.ACTION_ANNOUNCEMENTS_PREF,
                            PREFS_ANNOUNCEMENTS_ENABLED,
                            value);
    }

    /**
     * Broadcast the current value of the
     * <code>PREFS_ANNOUNCEMENTS_ENABLED</code> pref.
     */
    public static void broadcastAnnouncementsPref(final Context context) {
        final boolean value = getBooleanPref(context, PREFS_ANNOUNCEMENTS_ENABLED, true);
        broadcastAnnouncementsPref(context, value);
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
        final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
        return prefs.getBoolean(name, def);
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String prefName = preference.getKey();
        if (PREFS_MP_ENABLED.equals(prefName)) {
            showDialog((Boolean) newValue ? DIALOG_CREATE_MASTER_PASSWORD : DIALOG_REMOVE_MASTER_PASSWORD);

            // We don't want the "use master password" pref to change until the
            // user has gone through the dialog.
            return false;
        } else if (PREFS_MENU_CHAR_ENCODING.equals(prefName)) {
            setCharEncodingState(((String) newValue).equals("true"));
        } else if (PREFS_ANNOUNCEMENTS_ENABLED.equals(prefName)) {
            // Send a broadcast intent to the product announcements service, either to start or
            // to stop the repeated background checks.
            broadcastAnnouncementsPref(GeckoAppShell.getContext(), ((Boolean) newValue).booleanValue());
        } else if (PREFS_UPDATER_AUTODOWNLOAD.equals(prefName)) {
            org.mozilla.gecko.updater.UpdateServiceHelper.registerForUpdates(GeckoAppShell.getContext(), (String) newValue);
        } else if (PREFS_HEALTHREPORT_UPLOAD_ENABLED.equals(prefName)) {
            // The healthreport pref only lives in Android, so we do not persist
            // to Gecko, but we do broadcast intent to the health report
            // background uploader service, which will start or stop the
            // repeated background upload attempts.
            broadcastHealthReportUploadPref(GeckoAppShell.getContext(), ((Boolean) newValue).booleanValue());
        } else if (PREFS_GEO_REPORTING.equals(prefName)) {
            // Translate boolean value to int for geo reporting pref.
            newValue = ((Boolean) newValue) ? 1 : 0;
        }

        // Send Gecko-side pref changes to Gecko
        if (!TextUtils.isEmpty(prefName) && !prefName.startsWith(NON_PREF_PREFIX)) {
            PrefsHelper.setPref(prefName, newValue);
        }

        if (preference instanceof ListPreference) {
            // We need to find the entry for the new value
            int newIndex = ((ListPreference) preference).findIndexOfValue((String) newValue);
            CharSequence newEntry = ((ListPreference) preference).getEntries()[newIndex];
            ((ListPreference) preference).setSummary(newEntry);
        } else if (preference instanceof LinkPreference) {
            setResult(RESULT_CODE_EXIT_SETTINGS);
            finish();
        } else if (preference instanceof FontSizePreference) {
            final FontSizePreference fontSizePref = (FontSizePreference) preference;
            fontSizePref.setSummary(fontSizePref.getSavedFontSizeName());
        }

        return true;
    }

    private EditText getTextBox(int aHintText) {
        EditText input = new EditText(GeckoAppShell.getContext());
        int inputtype = InputType.TYPE_CLASS_TEXT;
        inputtype |= InputType.TYPE_TEXT_VARIATION_PASSWORD | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
        input.setInputType(inputtype);

        String hint = getResources().getString(aHintText);
        input.setHint(aHintText);
        return input;
    }

    private class PasswordTextWatcher implements TextWatcher {
        EditText input1 = null;
        EditText input2 = null;
        AlertDialog dialog = null;

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
        EditText input = null;
        AlertDialog dialog = null;

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
        AlertDialog dialog = null;
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
        JSONArray jsonPrefs = new JSONArray(prefs);

        return PrefsHelper.getPrefs(jsonPrefs, new PrefsHelper.PrefHandlerBase() {
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
                if (Build.VERSION.SDK_INT < 14) {
                    prefSetter = new CheckBoxPrefSetter();
                } else {
                    prefSetter = new TwoStatePrefSetter();
                }
                ThreadUtils.postToUiThread(new Runnable() {
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
                final CheckBoxPrefSetter prefSetter;
                if (PREFS_GEO_REPORTING.equals(prefName)) {
                    if (Build.VERSION.SDK_INT < 14) {
                        prefSetter = new CheckBoxPrefSetter();
                    } else {
                        prefSetter = new TwoStatePrefSetter();
                    }
                    ThreadUtils.postToUiThread(new Runnable() {
                        @Override
                        public void run() {
                            prefSetter.setBooleanPref(pref, value == 1);
                        }
                    });
                } else {
                    Log.w(LOGTAG, "Unhandled int value for pref [" + pref + "]");
                }
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

    private void registerEventListener(String event) {
        GeckoAppShell.getEventDispatcher().registerEventListener(event, this);
    }

    private void unregisterEventListener(String event) {
        GeckoAppShell.getEventDispatcher().unregisterEventListener(event, this);
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

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.HONEYCOMB) {
            intent.putExtra("resource", resource);
        } else {
            intent.putExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT, GeckoPreferenceFragment.class.getName());

            Bundle fragmentArgs = new Bundle();
            fragmentArgs.putString("resource", resource);
            intent.putExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArgs);
        }
    }
}
