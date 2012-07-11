/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.lang.CharSequence;
import java.util.ArrayList;

import android.app.Dialog;
import android.text.Editable;
import android.app.AlertDialog;
import android.os.Build;
import android.os.Bundle;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.content.Context;
import android.preference.*;
import android.preference.Preference.*;
import android.text.InputType;
import android.util.Log;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Toast;
import android.text.TextWatcher;
import android.text.TextUtils;
import android.content.DialogInterface;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

public class GeckoPreferences
    extends PreferenceActivity
    implements OnPreferenceChangeListener, GeckoEventListener
{
    private static final String LOGTAG = "GeckoPreferences";

    private ArrayList<String> mPreferencesList;
    private PreferenceScreen mPreferenceScreen;
    private static boolean sIsCharEncodingEnabled = false;
    private static final String NON_PREF_PREFIX = "android.not_a_preference.";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.xml.preferences);
        GeckoAppShell.registerGeckoEventListener("Preferences:Data", this);
        GeckoAppShell.registerGeckoEventListener("Sanitize:Finished", this);
   }

   @Override
   public void onWindowFocusChanged(boolean hasFocus) {
        if (!hasFocus)
            return;

        mPreferencesList = new ArrayList<String>();
        mPreferenceScreen = getPreferenceScreen();
        initGroups(mPreferenceScreen);
        initValues();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        GeckoAppShell.unregisterGeckoEventListener("Preferences:Data", this);
        GeckoAppShell.unregisterGeckoEventListener("Sanitize:Finished", this);
    }

    public void handleMessage(String event, JSONObject message) {
        try {
            if (event.equals("Preferences:Data")) {
                JSONArray jsonPrefs = message.getJSONArray("preferences");
                refresh(jsonPrefs);
            } else if (event.equals("Sanitize:Finished")) {
                boolean success = message.getBoolean("success");
                final int stringRes = success ? R.string.private_data_success : R.string.private_data_fail;
                final Context context = this;
                GeckoAppShell.getMainHandler().post(new Runnable () {
                    public void run() {
                        Toast.makeText(context, stringRes, Toast.LENGTH_SHORT).show();
                    }
                });
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    // Initialize preferences by sending the "Preferences:Get" command to Gecko
    private void initValues() {
        JSONArray jsonPrefs = new JSONArray(mPreferencesList);

        GeckoEvent event = GeckoEvent.createBroadcastEvent("Preferences:Get", jsonPrefs.toString());
        GeckoAppShell.sendEventToGecko(event);
    }

    private void initGroups(PreferenceGroup preferences) {
        final int count = preferences.getPreferenceCount();
        for (int i = 0; i < count; i++) {
            Preference pref = preferences.getPreference(i);
            if (pref instanceof PreferenceGroup)
                initGroups((PreferenceGroup)pref);
            else {
                pref.setOnPreferenceChangeListener(this);

                // Some Preference UI elements are not actually preferences,
                // but they require a key to work correctly. For example,
                // "Clear private data" requires a key for its state to be
                // saved when the orientation changes. It uses the
                // "android.not_a_preference.privacy.clear" key - which doesn't
                // exist in Gecko - to satisfy this requirement.
                String key = pref.getKey();
                if (key != null && !key.startsWith(NON_PREF_PREFIX))
                    mPreferencesList.add(pref.getKey());
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

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String prefName = preference.getKey();
        if (prefName != null && prefName.equals("privacy.masterpassword.enabled")) {
            showDialog((Boolean)newValue ? DIALOG_CREATE_MASTER_PASSWORD : DIALOG_REMOVE_MASTER_PASSWORD);
            return false;
        } else if (prefName != null && prefName.equals("browser.menu.showCharacterEncoding")) {
            setCharEncodingState(((String) newValue).equals("true"));
        }

        setPreference(prefName, newValue);
        if (preference instanceof ListPreference) {
            // We need to find the entry for the new value
            int newIndex = ((ListPreference)preference).findIndexOfValue((String) newValue);
            CharSequence newEntry = ((ListPreference)preference).getEntries()[newIndex];
            ((ListPreference)preference).setSummary(newEntry);
        }
        if (preference instanceof LinkPreference)
            finish();
        return true;
    }

    private EditText getTextBox(int aHintText) {
        EditText input = new EditText(GeckoApp.mAppContext);
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

        public void afterTextChanged(Editable s) {
            if (dialog == null)
                return;

            String text1 = input1.getText().toString();
            String text2 = input2.getText().toString();
            boolean disabled = TextUtils.isEmpty(text1) || TextUtils.isEmpty(text2) || !text1.equals(text2);
            dialog.getButton(DialogInterface.BUTTON_POSITIVE).setEnabled(!disabled);
        }

        public void beforeTextChanged(CharSequence s, int start, int count, int after) { }
        public void onTextChanged(CharSequence s, int start, int before, int count) { }
    }

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
                       .setView((View)linearLayout)
                       .setPositiveButton(R.string.button_ok, new DialogInterface.OnClickListener() {  
                            public void onClick(DialogInterface dialog, int which) {
                                JSONObject jsonPref = new JSONObject();
                                try {
                                    jsonPref.put("name", "privacy.masterpassword.enabled");
                                    jsonPref.put("type", "string");
                                    jsonPref.put("value", input1.getText().toString());
                    
                                    GeckoEvent event = GeckoEvent.createBroadcastEvent("Preferences:Set", jsonPref.toString());
                                    GeckoAppShell.sendEventToGecko(event);
                                } catch(Exception ex) {
                                    Log.e(LOGTAG, "Error setting masterpassword", ex);
                                }
                                return;
                            }
                        })
                        .setNegativeButton(R.string.button_cancel, new DialogInterface.OnClickListener() {  
                            public void onClick(DialogInterface dialog, int which) {
                                return;
                            }
                        });
                        dialog = builder.create();
                        dialog.setOnShowListener(new DialogInterface.OnShowListener() {
                            public void onShow(DialogInterface dialog) {
                                input1.setText("");
                                input2.setText("");
                                input1.requestFocus();
                            }
                        });

                        PasswordTextWatcher watcher = new PasswordTextWatcher(input1, input2, dialog);
                        input1.addTextChangedListener((TextWatcher)watcher);
                        input2.addTextChangedListener((TextWatcher)watcher);

                break;
            case DIALOG_REMOVE_MASTER_PASSWORD:
                final EditText input = getTextBox(R.string.masterpassword_password);
                linearLayout.addView(input);

                builder.setTitle(R.string.masterpassword_remove_title)
                       .setView((View)linearLayout)
                       .setPositiveButton(R.string.button_ok, new DialogInterface.OnClickListener() {  
                            public void onClick(DialogInterface dialog, int which) {
                                try {
                                    JSONObject jsonPref = new JSONObject();
                                    jsonPref.put("name", "privacy.masterpassword.enabled");
                                    jsonPref.put("type", "string");
                                    jsonPref.put("value", input.getText().toString());
                        
                                    GeckoEvent event = GeckoEvent.createBroadcastEvent("Preferences:Set", jsonPref.toString());
                                    GeckoAppShell.sendEventToGecko(event);
                                } catch(Exception ex) {
                                    Log.e(LOGTAG, "Error setting masterpassword", ex);
                                }
                                return;
                            }
                        })
                        .setNegativeButton(R.string.button_cancel, new DialogInterface.OnClickListener() {  
                            public void onClick(DialogInterface dialog, int which) {
                                return;
                            }
                        });
                        dialog = builder.create();
                        dialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
                            public void onDismiss(DialogInterface dialog) {
                                input.setText("");
                            }
                        });
                break;
            default:
                return null;
        }

        return dialog;
    }

    private void refresh(JSONArray jsonPrefs) {
        // enable all preferences once we have them from gecko
        GeckoAppShell.getMainHandler().post(new Runnable() {
            public void run() {
                mPreferenceScreen.setEnabled(true);
            }
        });

        try {
            if (mPreferenceScreen == null)
                return;

            final int length = jsonPrefs.length();
            for (int i = 0; i < length; i++) {
                JSONObject jPref = jsonPrefs.getJSONObject(i);
                final String prefName = jPref.getString("name");
                final String prefType = jPref.getString("type");
                final Preference pref = mPreferenceScreen.findPreference(prefName);

                if (pref instanceof CheckBoxPreference && "bool".equals(prefType)) {
                    final boolean value = jPref.getBoolean("value");
                    GeckoAppShell.getMainHandler().post(new Runnable() {
                        public void run() {
                            if (((CheckBoxPreference)pref).isChecked() != value)
                                ((CheckBoxPreference)pref).setChecked(value);
                        }
                    });
                } else if (pref instanceof EditTextPreference && "string".equals(prefType)) {
                    final String value = jPref.getString("value");
                    GeckoAppShell.getMainHandler().post(new Runnable() {
                        public void run() {
                            ((EditTextPreference)pref).setText(value);
                        }
                    });
                } else if (pref instanceof ListPreference && "string".equals(prefType)) {
                    final String value = jPref.getString("value");
                    GeckoAppShell.getMainHandler().post(new Runnable() {
                        public void run() {
                            ((ListPreference)pref).setValue(value);
                            // Set the summary string to the current entry
                            CharSequence selectedEntry = ((ListPreference)pref).getEntry();
                            ((ListPreference)pref).setSummary(selectedEntry);
                        }
                    });
                }
            }
        } catch (JSONException e) {
            Log.e(LOGTAG, "Problem parsing preferences response: ", e);
        }
    }

    // send the Preferences:Set message to Gecko
    public static void setPreference(String pref, Object value) {
        if (pref == null || pref.length() == 0)
            return;

        try {
            JSONObject jsonPref = new JSONObject();
            jsonPref.put("name", pref);
            if (value instanceof Boolean) {
                jsonPref.put("type", "bool");
                jsonPref.put("value", ((Boolean)value).booleanValue());
            }
            else if (value instanceof Integer) {
                jsonPref.put("type", "int");
                jsonPref.put("value", ((Integer)value).intValue());
            }
            else {
                jsonPref.put("type", "string");
                jsonPref.put("value", String.valueOf(value));
            }

            GeckoEvent event = GeckoEvent.createBroadcastEvent("Preferences:Set", jsonPref.toString());
            GeckoAppShell.sendEventToGecko(event);
        } catch (JSONException e) {
            Log.e(LOGTAG, "JSON exception: ", e);
        }
    }
}
