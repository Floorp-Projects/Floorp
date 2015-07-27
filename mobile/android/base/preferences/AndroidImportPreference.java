/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.R;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.RestrictedProfiles;
import org.mozilla.gecko.RestrictedProfiles.Restriction;

import java.util.Set;

import android.app.ProgressDialog;
import android.content.Context;
import android.preference.Preference;
import android.util.AttributeSet;
import android.util.Log;

class AndroidImportPreference extends MultiPrefMultiChoicePreference {
    private static final String LOGTAG = "AndroidImport";
    public static final String PREF_KEY = "android.not_a_preference.import_android";
    private static final String PREF_KEY_PREFIX = "import_android.data.";
    private final Context mContext;

    public static class Handler implements GeckoPreferences.PrefHandler {
        public boolean setupPref(Context context, Preference pref) {
            // Feature disabled on devices running Android M+ (Bug 1183559)
            return Versions.preM && RestrictedProfiles.isAllowed(context, Restriction.DISALLOW_IMPORT_SETTINGS);
        }

        public void onChange(Context context, Preference pref, Object newValue) { }
    }

    public AndroidImportPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    @Override
    protected void onDialogClosed(boolean positiveResult) {
        super.onDialogClosed(positiveResult);

        if (!positiveResult)
            return;

        boolean bookmarksChecked = false;
        boolean historyChecked = false;

        Set<String> values = getValues();

        for (String value : values) {
            // Import checkbox values are stored in Android prefs to
            // remember their check states. The key names are import_android.data.X
            String key = value.substring(PREF_KEY_PREFIX.length());
            if ("bookmarks".equals(key)) {
                bookmarksChecked = true;
            } else if ("history".equals(key)) {
                historyChecked = true;
            }
        }

        runImport(bookmarksChecked, historyChecked);
    }

    protected void runImport(final boolean doBookmarks, final boolean doHistory) {
        Log.i(LOGTAG, "Importing Android history/bookmarks");
        if (!doBookmarks && !doHistory) {
            return;
        }

        final String dialogTitle;
        if (doBookmarks && doHistory) {
            dialogTitle = mContext.getString(R.string.bookmarkhistory_import_both);
        } else if (doBookmarks) {
            dialogTitle = mContext.getString(R.string.bookmarkhistory_import_bookmarks);
        } else {
            dialogTitle = mContext.getString(R.string.bookmarkhistory_import_history);
        }

        final ProgressDialog dialog =
            ProgressDialog.show(mContext,
                                dialogTitle,
                                mContext.getString(R.string.bookmarkhistory_import_wait),
                                true);

        final Runnable stopCallback = new Runnable() {
            @Override
            public void run() {
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        dialog.dismiss();
                    }
                });
            }
        };

        ThreadUtils.postToBackgroundThread(
            // Constructing AndroidImport may need finding the profile,
            // which hits disk, so it needs to go into a Runnable too.
            new Runnable() {
                @Override
                public void run() {
                    new AndroidImport(mContext, stopCallback, doBookmarks, doHistory).run();
                }
            }
        );
    }
}
