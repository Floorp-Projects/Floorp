/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.db.PerProfileDatabases.DatabaseHelperFactory;

import android.content.Context;
import android.database.sqlite.SQLiteOpenHelper;

/**
 * Abstract class containing methods needed to make a SQLite-based content
 * provider with a database helper of type T, where one database helper is
 * held per profile.
 */
public abstract class PerProfileDatabaseProvider<T extends SQLiteOpenHelper> extends AbstractPerProfileDatabaseProvider {
    private PerProfileDatabases<T> databases;

    @Override
    protected PerProfileDatabases<T> getDatabases() {
        return databases;
    }

    protected abstract String getDatabaseName();

    /**
     * Creates and returns an instance of the appropriate DB helper.
     *
     * @param  context       to use to create the database helper
     * @param  databasePath  path to the DB file
     * @return               instance of the database helper
     */
    protected abstract T createDatabaseHelper(Context context, String databasePath);

    @Override
    public boolean onCreate() {
        synchronized (this) {
            databases = new PerProfileDatabases<T>(
                getContext(), getDatabaseName(), new DatabaseHelperFactory<T>() {
                    @Override
                    public T makeDatabaseHelper(Context context, String databasePath) {
                        final T helper = createDatabaseHelper(context, databasePath);
                        if (Versions.feature16Plus) {
                            helper.setWriteAheadLoggingEnabled(true);
                        }
                        return helper;
                    }
                });
        }

        return true;
    }
}
