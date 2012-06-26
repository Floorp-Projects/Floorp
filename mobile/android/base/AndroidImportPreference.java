/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.AlertDialog;
import android.app.AlertDialog.Builder;
import android.app.ProgressDialog;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.preference.DialogPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.util.AttributeSet;
import android.util.Log;
import android.util.SparseBooleanArray;

import org.mozilla.gecko.R;

class AndroidImportPreference extends DialogPreference {
    static final private String LOGTAG = "AndroidImport";
    private Context mContext;
    private boolean mBookmarksChecked;
    private boolean mHistoryChecked;

    public AndroidImportPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
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
            public void run() {
                GeckoApp.mAppContext.runOnUiThread(new Runnable() {
                    public void run() {
                        dialog.dismiss();
                    }
                });
            }
        };

        GeckoBackgroundThread.getHandler().post(
            // Constructing AndroidImport may need finding the profile,
            // which hits disk, so it needs to go into a Runnable too.
            new Runnable() {
                public void run() {
                    new AndroidImport(mContext, stopCallback, doBookmarks, doHistory).run();
                }
            }
        );
    }

    @Override
    protected void onPrepareDialogBuilder(Builder builder) {
        super.onPrepareDialogBuilder(builder);
        mBookmarksChecked = true;
        mHistoryChecked = true;
        builder.setMultiChoiceItems(R.array.pref_android_import_select,
                                    new boolean[] { mBookmarksChecked, mHistoryChecked },
            new DialogInterface.OnMultiChoiceClickListener() {
                @Override
                public void onClick(DialogInterface dialog,
                                    int which,
                                    boolean isChecked) {
                    Log.i(LOGTAG, "which = " + which + ", checked=" + isChecked);
                    if (which == 0) {
                        mBookmarksChecked = isChecked;
                    } else if (which == 1) {
                        mHistoryChecked = isChecked;
                    }
                }
            }
       );
    }

    @Override
    protected void onDialogClosed(boolean positiveResult) {
        if (!positiveResult)
            return;
        runImport(mBookmarksChecked, mHistoryChecked);
    }
}
