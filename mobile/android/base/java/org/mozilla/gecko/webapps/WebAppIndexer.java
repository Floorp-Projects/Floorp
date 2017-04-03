/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.webapps;

import java.util.ArrayList;

import android.content.Context;
import android.content.SharedPreferences;
import android.support.annotation.UiThread;

import org.mozilla.gecko.GeckoSharedPrefs;

/**
 * WebAppIndexer lets us create bookmarks that behave like android applications
 * we create 10 slots of WebAppN in the manifest, when a bookmark is launched
 * we take its manifest and assign it an index, subsequent launches of that
 * bookmark are given the same index and therefore reopen the previous activity.
 * We limit this to 10 spaces and recycle the least recently used index once
 * we hit the limit, this means if the user can actively use 10 webapps at the same
 * time, more than that and the last used will be replaced
 **/

public class WebAppIndexer {

    public static final String WEBAPP_CLASS = "org.mozilla.gecko.webapps.WebApps$WebApp";

    private static final String PREF_NUM_SAVED_ENTRIES = "WebAppIndexer.numActivities";
    private static final String PREF_ACTIVITY_INDEX = "WebAppIndexer.index";
    private static final String PREF_ACTIVITY_ID = "WebAppIndexer.manifest";

    private static final int MAX_ACTIVITIES = 10;
    private static final int INVALID_INDEX = -1;

    private ArrayList<ActivityEntry> mActivityList = new ArrayList<>();

    private static class ActivityEntry {
        public final int index;
        public final String manifest;
        ActivityEntry(int _index, String _manifest) {
            index = _index;
            manifest = _manifest;
        }
    }

    private WebAppIndexer() { }
    private final static WebAppIndexer INSTANCE = new WebAppIndexer();
    public static WebAppIndexer getInstance() {
        return INSTANCE;
    }

    public int getIndexForManifest(String manifest, Context context) {

        if (mActivityList.size() == 0) {
            loadActivityList(context);
        }

        int index = getManifestIndex(manifest);

        // If we havent assigned this manifest an index then reassign the
        // least recently used index
        if (index == INVALID_INDEX) {
            index = mActivityList.get(0).index;
            final ActivityEntry newEntry = new ActivityEntry(index, manifest);
            mActivityList.set(0, newEntry);
        }

        // Put the index at the back of the queue to be recycled
        markActivityUsed(index, manifest, context);

        return index;
    }

    private int getManifestIndex(String manifest) {
        for (int i = mActivityList.size() - 1; i >= 0; i--) {
            if (manifest.equals(mActivityList.get(i).manifest)) {
                return mActivityList.get(i).index;
            }
        }
        return INVALID_INDEX;
    }

    private void markActivityUsed(int index, String manifest, Context context) {
        final int elementIndex = findActivityElement(index);
        final ActivityEntry updatedEntry = new ActivityEntry(index, manifest);
        mActivityList.remove(elementIndex);
        mActivityList.add(updatedEntry);
        storeActivityList(context);
    }

    private int findActivityElement(int index) {
        for (int elementIndex = 0; elementIndex < mActivityList.size(); elementIndex++) {
            if (mActivityList.get(elementIndex).index == index) {
                return elementIndex;
            }
        }
        return INVALID_INDEX;
    }

    // Store the list of assigned indexes in sharedPrefs because the LauncherActivity
    // is likely to be killed and restarted between webapp launches
    @UiThread
    private void storeActivityList(Context context) {
        final SharedPreferences prefs = GeckoSharedPrefs.forProfile(context);
        final SharedPreferences.Editor editor = prefs.edit();
        clearActivityListPrefs(prefs, editor);
        editor.putInt(PREF_NUM_SAVED_ENTRIES, mActivityList.size());
        for (int i = 0; i < mActivityList.size(); ++i) {
            editor.putInt(PREF_ACTIVITY_INDEX + i, mActivityList.get(i).index);
            editor.putString(PREF_ACTIVITY_ID + i, mActivityList.get(i).manifest);
        }
        editor.apply();
    }

    private void clearActivityListPrefs(SharedPreferences prefs, SharedPreferences.Editor editor) {
        int count = prefs.getInt(PREF_NUM_SAVED_ENTRIES, 0);

        for (int i = 0; i < count; ++i) {
            editor.remove(PREF_ACTIVITY_INDEX + i);
            editor.remove(PREF_ACTIVITY_ID + i);
        }
        editor.remove(PREF_NUM_SAVED_ENTRIES);
    }

    @UiThread
    private void loadActivityList(Context context) {
        final SharedPreferences prefs = GeckoSharedPrefs.forProfile(context);
        final int numSavedEntries = prefs.getInt(PREF_NUM_SAVED_ENTRIES, 0);
        for (int i = 0; i < numSavedEntries; ++i) {
          int index = prefs.getInt(PREF_ACTIVITY_INDEX + i, i);
          String manifest = prefs.getString(PREF_ACTIVITY_ID + i, null);
          ActivityEntry entry = new ActivityEntry(index, manifest);
          mActivityList.add(entry);
        }

        // Fill rest of the list with null spacers to be assigned
        final int leftToFill = MAX_ACTIVITIES - mActivityList.size();
        for (int i = 0; i < leftToFill; ++i) {
            ActivityEntry entry = new ActivityEntry(i, null);
            mActivityList.add(entry);
        }
    }
}
