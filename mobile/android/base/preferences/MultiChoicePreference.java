/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import org.mozilla.gecko.R;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.AlertDialog;
import android.app.AlertDialog.Builder;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.TypedArray;
import android.preference.DialogPreference;
import android.util.AttributeSet;
import android.widget.Button;

class MultiChoicePreference extends DialogPreference {
    private static final String LOGTAG = "GeckoMultiChoicePreference";

    private boolean mValues[];
    private boolean mPrevValues[];
    private CharSequence mEntryKeys[];
    private CharSequence mEntries[];
    private CharSequence mInitialValues[];

    public MultiChoicePreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.MultiChoicePreference);
        mEntries = a.getTextArray(R.styleable.MultiChoicePreference_entries);
        mEntryKeys = a.getTextArray(R.styleable.MultiChoicePreference_entryKeys);
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
     * {@link #setEntryKeys(CharSequence[])} and
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
     * Sets the preference keys for preferences shown in the list.
     *
     * @param entryKeys The entry keys.
     */
    public void setEntryKeys(CharSequence[] entryKeys) {
        mEntryKeys = entryKeys.clone();
        loadPersistedValues();
    }

    /**
     * @param entryKeysResId The entryKeys array as a resource.
     */
    public void setEntryKeys(int entryKeysResId) {
        setEntryKeys(getContext().getResources().getTextArray(entryKeysResId));
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
     * The list of keys corresponding to each preference.
     * 
     * @return The array of keys.
     */
    public CharSequence[] getEntryKeys() {
        return mEntryKeys.clone();
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

    /**
     * The list of values for each preference. These values are updated after
     * the dialog has been displayed.
     * 
     * @return The array of values.
     */
    public boolean[] getValues() {
        return mValues.clone();
    }

    @Override
    protected void onPrepareDialogBuilder(Builder builder) {
        if (mEntries == null || mEntryKeys == null || mInitialValues == null) {
            throw new IllegalStateException(
                    "MultiChoicePreference requires entries, entryKeys, and initialValues arrays.");
        }

        if (mEntries.length != mEntryKeys.length || mEntryKeys.length != mInitialValues.length) {
            throw new IllegalStateException(
                    "MultiChoicePreference entries, entryKeys, and initialValues arrays must be the same length");
        }

        builder.setMultiChoiceItems(mEntries, mValues, new DialogInterface.OnMultiChoiceClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which, boolean val) {
                // mValues is automatically updated when checkboxes are clicked

                // enable positive button only if at least one item is checked
                boolean enabled = false;
                for (int i = 0; i < mValues.length; i++) {
                    if (mValues[i]) {
                        enabled = true;
                        break;
                    }
                }
                Button button = ((AlertDialog) dialog).getButton(DialogInterface.BUTTON_POSITIVE);
                if (button.isEnabled() != enabled)
                    button.setEnabled(enabled);
            }
        });
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

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                for (int i = 0; i < mEntryKeys.length; i++) {
                    String key = mEntryKeys[i].toString();
                    persistBoolean(key, mValues[i]);
                }
            }
        });
    }

    protected boolean persistBoolean(String key, boolean value) {
        if (isPersistent()) {
            if (value == getPersistedBoolean(!value)) {
                // It's already there, so the same as persisting
                return true;
            }
            
            GeckoSharedPrefs.forApp(getContext())
                            .edit().putBoolean(key, value).commit();
            return true;
        }
        return false;
    }

    protected boolean getPersistedBoolean(String key, boolean defaultReturnValue) {
        if (!isPersistent())
            return defaultReturnValue;
        
        return GeckoSharedPrefs.forApp(getContext())
                               .getBoolean(key, defaultReturnValue);
    }

    /**
     * Loads persistent prefs from shared preferences. If the preferences
     * aren't persistent or haven't yet been stored, they will be set to their
     * initial values.
     */
    private void loadPersistedValues() {
        if (mEntryKeys == null || mInitialValues == null)
            return;

        final int entryCount = mEntryKeys.length;
        if (entryCount != mEntries.length || entryCount != mInitialValues.length) {
            throw new IllegalStateException(
                    "MultiChoicePreference entryKeys and initialValues arrays must be the same length");
        }

        mValues = new boolean[entryCount];
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                for (int i = 0; i < entryCount; i++) {
                    String key = mEntryKeys[i].toString();
                    boolean initialValue = mInitialValues[i].equals("true");
                    mValues[i] = getPersistedBoolean(key, initialValue);
                }
                mPrevValues = mValues.clone();
            }
        });
    }
}
