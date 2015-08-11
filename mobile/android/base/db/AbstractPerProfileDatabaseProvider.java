/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import org.mozilla.gecko.annotation.RobocopTarget;

import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.net.Uri;

/**
 * The base class for ContentProviders that wish to use a different DB
 * for each profile.
 *
 * This class has logic shared between ordinary per-profile CPs and
 * those that wish to share DB connections between CPs.
 */
public abstract class AbstractPerProfileDatabaseProvider extends AbstractTransactionalProvider {

    /**
     * Extend this to provide access to your own map of shared databases. This
     * is a method so that your subclass doesn't collide with others!
     */
    protected abstract PerProfileDatabases<? extends SQLiteOpenHelper> getDatabases();

    /*
     * Fetches a readable database based on the profile indicated in the
     * passed URI. If the URI does not contain a profile param, the default profile
     * is used.
     *
     * @param uri content URI optionally indicating the profile of the user
     * @return    instance of a readable SQLiteDatabase
     */
    @Override
    protected SQLiteDatabase getReadableDatabase(Uri uri) {
        String profile = null;
        if (uri != null) {
            profile = uri.getQueryParameter(BrowserContract.PARAM_PROFILE);
        }

        return getDatabases().getDatabaseHelperForProfile(profile, isTest(uri)).getReadableDatabase();
    }

    /*
     * Fetches a writable database based on the profile indicated in the
     * passed URI. If the URI does not contain a profile param, the default profile
     * is used
     *
     * @param uri content URI optionally indicating the profile of the user
     * @return    instance of a writable SQLiteDatabase
     */
    @Override
    protected SQLiteDatabase getWritableDatabase(Uri uri) {
        String profile = null;
        if (uri != null) {
            profile = uri.getQueryParameter(BrowserContract.PARAM_PROFILE);
        }

        return getDatabases().getDatabaseHelperForProfile(profile, isTest(uri)).getWritableDatabase();
    }

    protected SQLiteDatabase getWritableDatabaseForProfile(String profile, boolean isTest) {
        return getDatabases().getDatabaseHelperForProfile(profile, isTest).getWritableDatabase();
    }

    /**
     * This method should ONLY be used for testing purposes.
     *
     * @param uri content URI optionally indicating the profile of the user
     * @return    instance of a writable SQLiteDatabase
     */
    @Override
    @RobocopTarget
    public SQLiteDatabase getWritableDatabaseForTesting(Uri uri) {
        return getWritableDatabase(uri);
    }
}
