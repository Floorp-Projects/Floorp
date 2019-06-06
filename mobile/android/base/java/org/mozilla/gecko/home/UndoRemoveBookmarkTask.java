/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.support.design.widget.Snackbar;

import org.mozilla.gecko.R;
import org.mozilla.gecko.SnackbarBuilder;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.reader.SavedReaderViewHelper;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UIAsyncTask;

import java.lang.ref.WeakReference;

public class UndoRemoveBookmarkTask extends UIAsyncTask.WithoutParams<Void> {
    private static final int NONEXISTENT_POSITION = -1;

    private final WeakReference<Activity> activityWeakReference;
    private final Context context;
    private final HomeContextMenuInfo info;
    private final int position;
    private final BrowserDB db;

    /**
     * Remove bookmark/history/reading list type item, and also unpin the
     * Top Sites grid item at index <code>position</code>.
     */
    public UndoRemoveBookmarkTask(Activity activity, HomeContextMenuInfo info, int position) {
        super(ThreadUtils.getBackgroundHandler());

        this.activityWeakReference = new WeakReference<>(activity);
        this.context = activity.getApplicationContext();
        this.info = info;
        this.position = position;
        this.db = BrowserDB.from(context);
    }

    @Override
    public Void doInBackground() {
        ContentResolver cr = context.getContentResolver();

        if (position > NONEXISTENT_POSITION) {
            db.pinSite(cr, info.url, info.title, position);
            if (!db.hideSuggestedSite(info.url)) {
                cr.notifyChange(BrowserContract.SuggestedSites.CONTENT_URI, null);
            }
        }

        db.updateSoftDeleteForBookmarkWithId(cr, info.bookmarkId, false);

        return null;
    }

    @Override
    public void onPostExecute(Void result) {
        final Activity activity = activityWeakReference.get();
        if (activity == null || activity.isFinishing()) {
            return;
        }

        SnackbarBuilder.builder(activity)
                .message(R.string.bookmark_added)
                .duration(Snackbar.LENGTH_LONG)
                .buildAndShow();
    }
}

