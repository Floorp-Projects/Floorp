/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import java.io.File;
import java.util.HashMap;

import org.mozilla.gecko.GeckoProfile;

import android.content.Context;
import android.database.sqlite.SQLiteOpenHelper;
import android.text.TextUtils;

/**
 * Manages a set of per-profile database storage helpers.
 */
public class PerProfileDatabases<T extends SQLiteOpenHelper> {

    private final HashMap<String, T> mStorages = new HashMap<String, T>();

    private final Context mContext;
    private final String mDatabaseName;
    private final DatabaseHelperFactory<T> mHelperFactory;

    public interface DatabaseHelperFactory<T> {
        public T makeDatabaseHelper(Context context, String databasePath);
    }

    public PerProfileDatabases(final Context context, final String databaseName, final DatabaseHelperFactory<T> helperFactory) {
        mContext = context;
        mDatabaseName = databaseName;
        mHelperFactory = helperFactory;
    }

    public String getDatabasePathForProfile(String profile) {
        final File profileDir = GeckoProfile.get(mContext, profile).getDir();
        if (profileDir == null) {
            return null;
        }

        return new File(profileDir, mDatabaseName).getAbsolutePath();
    }

    public T getDatabaseHelperForProfile(String profile) {
        return getDatabaseHelperForProfile(profile, false);
    }

    public T getDatabaseHelperForProfile(String profile, boolean isTest) {
        // Always fall back to default profile if none has been provided.
        if (TextUtils.isEmpty(profile)) {
            profile = GeckoProfile.get(mContext).getName();
        }

        synchronized (this) {
            if (mStorages.containsKey(profile)) {
                return mStorages.get(profile);
            }

            final String databasePath = isTest ? mDatabaseName : getDatabasePathForProfile(profile);
            if (databasePath == null) {
                throw new IllegalStateException("Database path is null for profile: " + profile);
            }

            final T helper = mHelperFactory.makeDatabaseHelper(mContext, databasePath);
            DBUtils.ensureDatabaseIsNotLocked(helper, databasePath);

            mStorages.put(profile, helper);
            return helper;
        }
    }
}
