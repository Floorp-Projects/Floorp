/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import org.mozilla.gecko.R;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.util.PrefUtils;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.AlertDialog.Builder;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.TypedArray;
import android.content.SharedPreferences;
import android.preference.DialogPreference;
import android.util.AttributeSet;

import java.util.HashSet;
import java.util.Set;

class MultiChoicePreference extends DialogPreference implements DialogInterface.OnMultiChoiceClickListener {
    private static final String LOGTAG = "GeckoMultiChoicePreference";

    private boolean mValues[];
    private boolean mPrevValues[];
    private CharSequence mEntryValues[];
    private CharSequence mEntries[];
    private CharSequence mInitialValues[];

    public MultiChoicePreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.MultiChoicePreference);
        mEntries = a.getTextArray(R.styleable.MultiChoicePreference_entries);
        mEntryValues = a.getTextArray(R.styleable.MultiChoicePreference_entryValues);
        mInitialValues = a.getTextArray(R.styleable.MultiChoicePreference_initialValues);
        a.recycle();

        loadPersistedValues();
    }

    public MultiChoicePreference(Context context) {
        this(context, null);
    }

    /**
     * Sets the human-readable entries to be shown in the list. This will be
     * shown in subsequent dialogs.
     * <p>
     * Each entry must have a corresponding index in
     * {@link #setEntryValues(CharSequence[])} and
     * {@link #setInitialValues(CharSequence[])}.
     *
     * @param entries The entries.
     */
    public void setEntries(CharSequence[] entries) {
        mEntries = entries.clone();
    }
    
    /**
     * @param entriesResId The entries array as a resource.
     */
    public void setEntries(int entriesResId) {
        setEntries(getContext().getResources().getTextArray(entriesResId));
    }

    /**
     * Sets the preference values for preferences shown in the list.
     *
     * @param entryValues The entry values.
     */
    public void setEntryValues(CharSequence[] entryValues) {
        mEntryValues = entryValues.clone();
        loadPersistedValues();
    }

    /**
     * Entry values define a separate pref for each row in the dialog.
     *
     * @param entryValuesResId The entryValues array as a resource.
     */
    public void setEntryValues(int entryValuesResId) {
        setEntryValues(getContext().getResources().getTextArray(entryValuesResId));
    }

    /**
     * The array of initial entry values in this list. Each entryValue
     * corresponds to an entryKey. These values are used if a) the preference
     * isn't persisted, or b) the preference is persisted but hasn't yet been
     * set.
     *
     * @param initialValues The entry values
     */
    public void setInitialValues(CharSequence[] initialValues) {
        mInitialValues = initialValues.clone();
        loadPersistedValues();
    }

    /**
     * @param initialValuesResId The initialValues array as a resource.
     */
    public void setInitialValues(int initialValuesResId) {
        setInitialValues(getContext().getResources().getTextArray(initialValuesResId));
    }

    /**
     * The list of translated strings corresponding to each preference.
     * 
     * @return The array of entries.
     */
    public CharSequence[] getEntries() {
        return mEntries.clone();
    }

    /**
     * The list of values corresponding to each preference.
     * 
     * @return The array of values.
     */
    public CharSequence[] getEntryValues() {
        return mEntryValues.clone();
    }

    /**
     * The list of initial values for each preference. Each string in this list
     * should be either "true" or "false".
     * 
     * @return The array of initial values.
     */
    public CharSequence[] getInitialValues() {
        return mInitialValues.clone();
    }

    public void setValue(final int i, final boolean value) {
        mValues[i] = value;
        mPrevValues = mValues.clone();
    }

    /**
     * The list of values for each preference. These values are updated after
     * the dialog has been displayed.
     * 
     * @return The array of values.
     */
    public Set<String> getValues() {
        final Set<String> values = new HashSet<String>();

        if (mValues == null) {
            return values;
        }

        for (int i = 0; i < mValues.length; i++) {
            if (mValues[i]) {
                values.add(mEntryValues[i].toString());
            }
        }

        return values;
    }

    @Override
    public void onClick(DialogInterface dialog, int which, boolean val) {
    }

    @Override
    protected void onPrepareDialogBuilder(Builder builder) {
        if (mEntries == null || mInitialValues == null || mEntryValues == null) {
            throw new IllegalStateException(
                    "MultiChoicePreference requires entries, entryValues, and initialValues arrays.");
        }

        if (mEntries.length != mEntryValues.length || mEntries.length != mInitialValues.length) {
            throw new IllegalStateException(
                    "MultiChoicePreference entries, entryValues, and initialValues arrays must be the same length");
        }

        builder.setMultiChoiceItems(mEntries, mValues, this);
    }

    @Override
    protected void onDialogClosed(boolean positiveResult) {
        if (mPrevValues == null || mInitialValues == null) {
            // Initialization is done asynchronously, so these values may not
            // have been set before the dialog was closed.
            return;
        }

        if (!positiveResult) {
            // user cancelled; reset checkbox values to their previous state
            mValues = mPrevValues.clone();
            return;
        } else {
            mPrevValues = mValues.clone();
        }

        if (!callChangeListener(getValues())) {
            return;
        }

        persist();
    }

    /* Persists the current data stored by this pref to SharedPreferences. */
    public boolean persist() {
        if (isPersistent()) {
            final SharedPreferences.Editor edit = GeckoSharedPrefs.forProfile(getContext()).edit();
            final boolean res = persist(edit);
            edit.apply();
            return res;
        }

        return false;
    }

    /* Internal persist method. Take an edit so that multiple prefs can be persisted in a single commit. */
    protected boolean persist(SharedPreferences.Editor edit) {
        if (isPersistent()) {
            Set<String> vals = getValues();
            PrefUtils.putStringSet(edit, getKey(), vals);
            return true;
        }

        return false;
    }

    /* Returns a list of EntryValues that are currently enabled. */
    public Set<String> getPersistedStrings(Set<String> defaultVal) {
        if (!isPersistent()) {
            return defaultVal;
        }

        final SharedPreferences prefs = GeckoSharedPrefs.forProfile(getContext());
        return PrefUtils.getStringSet(prefs, getKey(), defaultVal);
    }

    /**
     * Loads persistent prefs from shared preferences. If the preferences
     * aren't persistent or haven't yet been stored, they will be set to their
     * initial values.
     */
    protected void loadPersistedValues() {
        final int entryCount = mInitialValues.length;
        mValues = new boolean[entryCount];

        if (entryCount != mEntries.length || entryCount != mEntryValues.length) {
            throw new IllegalStateException(
                    "MultiChoicePreference entryValues and initialValues arrays must be the same length");
        }

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final Set<String> stringVals = getPersistedStrings(null);

                for (int i = 0; i < entryCount; i++) {
                    if (stringVals != null) {
                        mValues[i] = stringVals.contains(mEntryValues[i]);
                    } else {
                        final boolean defaultVal = mInitialValues[i].equals("true");
                        mValues[i] = defaultVal;
                    }
                }

                mPrevValues = mValues.clone();
            }
        });
    }
}
