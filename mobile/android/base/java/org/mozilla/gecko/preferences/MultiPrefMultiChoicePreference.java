/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import org.mozilla.gecko.R;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.TypedArray;
import android.content.SharedPreferences;
import android.widget.Button;
import android.util.AttributeSet;
import android.util.Log;

import java.util.Set;

/* Provides backwards compatibility for some old multi-choice pref types used by Gecko.
 * This will import the old data from the old prefs the first time it is run.
 */
class MultiPrefMultiChoicePreference extends MultiChoicePreference {
    private static final String LOGTAG = "GeckoMultiPrefPreference";
    private static final String IMPORT_SUFFIX = "_imported_";
    private final CharSequence[] keys;

    public MultiPrefMultiChoicePreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        final TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.MultiPrefMultiChoicePreference);
        keys = a.getTextArray(R.styleable.MultiPrefMultiChoicePreference_entryKeys);
        a.recycle();

        loadPersistedValues();
    }

    // Helper method for reading a boolean pref.
    private boolean getPersistedBoolean(SharedPreferences prefs, String key, boolean defaultReturnValue) {
        if (!isPersistent()) {
            return defaultReturnValue;
        }

        return prefs.getBoolean(key, defaultReturnValue);
    }

    // Overridden to do a one time import for the old preference type to the new one.
    @Override
    protected synchronized void loadPersistedValues() {
        // This will load the new pref if it exists.
        super.loadPersistedValues();

        // First check if we've already done the import the old data. If so, nothing to load.
        final SharedPreferences prefs = GeckoSharedPrefs.forApp(getContext());
        final boolean imported = getPersistedBoolean(prefs, getKey() + IMPORT_SUFFIX, false);
        if (imported) {
            return;
        }

        // Load the data we'll need to find the old style prefs
        final CharSequence[] init = getInitialValues();
        final CharSequence[] entries = getEntries();
        if (keys == null || init == null) {
            return;
        }

        final int entryCount = keys.length;
        if (entryCount != entries.length || entryCount != init.length) {
            throw new IllegalStateException("MultiChoicePreference entryKeys and initialValues arrays must be the same length");
        }

        // Now iterate through the entries on a background thread.
        final SharedPreferences.Editor edit = prefs.edit();
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                try {
                    // Use one editor to batch as many changes as we can.
                    for (int i = 0; i < entryCount; i++) {
                        String key = keys[i].toString();
                        boolean initialValue = "true".equals(init[i]);
                        boolean val = getPersistedBoolean(prefs, key, initialValue);

                        // Save the pref and remove the old preference.
                        setValue(i, val);
                        edit.remove(key);
                    }

                    persist(edit);
                    edit.putBoolean(getKey() + IMPORT_SUFFIX, true);
                    edit.apply();
                } catch (Exception ex) {
                    Log.i(LOGTAG, "Err", ex);
                }
            }
        });
    }


    @Override
    public void onClick(DialogInterface dialog, int which, boolean val) {
        // enable positive button only if at least one item is checked
        boolean enabled = false;
        final Set<String> values = getValues();

        enabled = (values.size() > 0);
        final Button button = ((AlertDialog) dialog).getButton(DialogInterface.BUTTON_POSITIVE);
        if (button.isEnabled() != enabled) {
            button.setEnabled(enabled);
        }
    }

}
