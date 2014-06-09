/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import java.io.File;
import java.util.HashMap;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.sqlite.SQLiteBridge;
import org.mozilla.gecko.sqlite.SQLiteBridgeException;

import android.content.ContentProvider;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Log;

/*
 * Provides a basic ContentProvider that sets up and sends queries through
 * SQLiteBridge. Content providers should extend this by setting the appropriate
 * table and version numbers in onCreate, and implementing the abstract methods:
 *
 *  public abstract String getTable(Uri uri);
 *  public abstract String getSortOrder(Uri uri, String aRequested);
 *  public abstract void setupDefaults(Uri uri, ContentValues values);
 *  public abstract void initGecko();
 */

public abstract class SQLiteBridgeContentProvider extends ContentProvider {
    private static final String ERROR_MESSAGE_DATABASE_IS_LOCKED = "Can't step statement: (5) database is locked";

    private HashMap<String, SQLiteBridge> mDatabasePerProfile;
    protected Context mContext = null;
    private final String mLogTag;

    protected SQLiteBridgeContentProvider(String logTag) {
        mLogTag = logTag;
    }

    /**
     * Subclasses must override this to allow error reporting code to compose
     * the correct histogram name.
     *
     * Ensure that you define the new histograms if you define a new class!
     */
    protected abstract String getTelemetryPrefix();

    /**
     * Errors are recorded in telemetry using an enumerated histogram.
     *
     * <https://developer.mozilla.org/en-US/docs/Mozilla/Performance/
     * Adding_a_new_Telemetry_probe#Choosing_a_Histogram_Type>
     *
     * These are the allowable enumeration values. Keep these in sync with the
     * histogram definition!
     *
     */
    private static enum TelemetryErrorOp {
        BULKINSERT (0),
        DELETE     (1),
        INSERT     (2),
        QUERY      (3),
        UPDATE     (4);

        private final int bucket;

        TelemetryErrorOp(final int bucket) {
            this.bucket = bucket;
        }

        public int getBucket() {
            return bucket;
        }
    }

    @Override
    public void shutdown() {
        if (mDatabasePerProfile == null) {
            return;
        }

        synchronized (this) {
            for (SQLiteBridge bridge : mDatabasePerProfile.values()) {
                if (bridge != null) {
                    try {
                        bridge.close();
                    } catch (Exception ex) { }
                }
            }
            mDatabasePerProfile = null;
        }
    }

    @Override
    public void finalize() {
        shutdown();
    }

    /**
     * Return true of the query is from Firefox Sync.
     * @param uri query URI
     */
    public static boolean isCallerSync(Uri uri) {
        String isSync = uri.getQueryParameter(BrowserContract.PARAM_IS_SYNC);
        return !TextUtils.isEmpty(isSync);
    }

    private SQLiteBridge getDB(Context context, final String databasePath) {
        SQLiteBridge bridge = null;

        boolean dbNeedsSetup = true;
        try {
            String resourcePath = context.getPackageResourcePath();
            GeckoLoader.loadSQLiteLibs(context, resourcePath);
            GeckoLoader.loadNSSLibs(context, resourcePath);
            bridge = SQLiteBridge.openDatabase(databasePath, null, 0);
            int version = bridge.getVersion();
            dbNeedsSetup = version != getDBVersion();
        } catch (SQLiteBridgeException ex) {
            // close the database
            if (bridge != null) {
                bridge.close();
            }

            // this will throw if the database can't be found
            // we should attempt to set it up if Gecko is running
            dbNeedsSetup = true;
            Log.e(mLogTag, "Error getting version ", ex);

            // if Gecko is not running, we should bail out. Otherwise we try to
            // let Gecko build the database for us
            if (!GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoRunning)) {
                Log.e(mLogTag, "Can not set up database. Gecko is not running");
                return null;
            }
        }

        // If the database is not set up yet, or is the wrong schema version, we send an initialize
        // call to Gecko. Gecko will handle building the database file correctly, as well as any
        // migrations that are necessary
        if (dbNeedsSetup) {
            bridge = null;
            initGecko();
        }
        return bridge;
    }
    
    /**
     * Returns the absolute path of a database file depending on the specified profile and dbName.
     * @param profile
     *          the profile whose dbPath must be returned
     * @param dbName
     *          the name of the db file whose absolute path must be returned
     * @return the absolute path of the db file or <code>null</code> if it was not possible to retrieve a valid path
     *
     */
    private String getDatabasePathForProfile(String profile, String dbName) {
        // Depends on the vagaries of GeckoProfile.get, so null check for safety.
        File profileDir = GeckoProfile.get(mContext, profile).getDir();
        if (profileDir == null) {
            return null;
        }

        String databasePath = new File(profileDir, dbName).getAbsolutePath();
        return databasePath;
    }

    /**
     * Returns a SQLiteBridge object according to the specified profile id and to the name of db related to the
     * current provider instance.
     * @param profile
     *          the id of the profile to be used to retrieve the related SQLiteBridge
     * @return the <code>SQLiteBridge</code> related to the specified profile id or <code>null</code> if it was 
     *         not possible to retrieve a valid SQLiteBridge
     */
    private SQLiteBridge getDatabaseForProfile(String profile) {
        if (TextUtils.isEmpty(profile)) {
            profile = GeckoProfile.get(mContext).getName();
            Log.d(mLogTag, "No profile provided, using '" + profile + "'");
        }

        final String dbName = getDBName();
        String mapKey = profile + "/" + dbName;

        SQLiteBridge db = null;
        synchronized (this) {
            db = mDatabasePerProfile.get(mapKey);
            if (db != null) {
                return db;
            }
            final String dbPath = getDatabasePathForProfile(profile, dbName);
            if (dbPath == null) {   
                Log.e(mLogTag, "Failed to get a valid db path for profile '" + profile + "'' dbName '" + dbName + "'");
                return null;
            }
            db = getDB(mContext, dbPath);
            if (db != null) {
                mDatabasePerProfile.put(mapKey, db);
            }
        }
        return db;
    }

    /**
     * Returns a SQLiteBridge object according to the specified profile path and to the name of db related to the
     * current provider instance.
     * @param profilePath
     *          the profilePath to be used to retrieve the related SQLiteBridge
     * @return the <code>SQLiteBridge</code> related to the specified profile path or <code>null</code> if it was
     *         not possible to retrieve a valid <code>SQLiteBridge</code>
     */
    private SQLiteBridge getDatabaseForProfilePath(String profilePath) {
        File profileDir = new File(profilePath, getDBName());
        final String dbPath = profileDir.getPath();
        return getDatabaseForDBPath(dbPath);
    }

    /**
     * Returns a SQLiteBridge object according to the specified file path.
     * @param dbPath
     *          the path of the file to be used to retrieve the related SQLiteBridge
     * @return the <code>SQLiteBridge</code> related to the specified file path or <code>null</code> if it was  
     *         not possible to retrieve a valid <code>SQLiteBridge</code>
     *
     */
    private SQLiteBridge getDatabaseForDBPath(String dbPath) {
        SQLiteBridge db = null;
        synchronized (this) {
            db = mDatabasePerProfile.get(dbPath);
            if (db != null) {
                return db;
            }
            db = getDB(mContext, dbPath);
            if (db != null) {
                mDatabasePerProfile.put(dbPath, db);
            }
        }
        return db;
    }

    /**
     * Returns a SQLiteBridge object to be used to perform operations on the given <code>Uri</code>.
     * @param uri
     *          the <code>Uri</code> to be used to retrieve the related SQLiteBridge
     * @return a <code>SQLiteBridge</code> object to be used on the given uri or <code>null</code> if it was 
     *         not possible to retrieve a valid <code>SQLiteBridge</code>
     *
     */
    private SQLiteBridge getDatabase(Uri uri) {
        String profile = null;
        String profilePath = null;

        profile = uri.getQueryParameter(BrowserContract.PARAM_PROFILE);
        profilePath = uri.getQueryParameter(BrowserContract.PARAM_PROFILE_PATH);

        // Testing will specify the absolute profile path
        if (profilePath != null) {
            return getDatabaseForProfilePath(profilePath);
        }
        return getDatabaseForProfile(profile);
    }

    @Override
    public boolean onCreate() {
        mContext = getContext();
        synchronized (this) {
            mDatabasePerProfile = new HashMap<String, SQLiteBridge>();
        }
        return true;
    }

    @Override
    public String getType(Uri uri) {
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        int deleted = 0;
        final SQLiteBridge db = getDatabase(uri);
        if (db == null) {
            return deleted;
        }

        try {
            deleted = db.delete(getTable(uri), selection, selectionArgs);
        } catch (SQLiteBridgeException ex) {
            reportError(ex, TelemetryErrorOp.DELETE);
            throw ex;
        }

        return deleted;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        long id = -1;
        final SQLiteBridge db = getDatabase(uri);

        // If we can not get a SQLiteBridge instance, its likely that the database
        // has not been set up and Gecko is not running. We return null and expect
        // callers to try again later
        if (db == null) {
            return null;
        }

        setupDefaults(uri, values);

        boolean useTransaction = !db.inTransaction();
        try {
            if (useTransaction) {
                db.beginTransaction();
            }

            // onPreInsert does a check for the item in the deleted table in some cases
            // so we put it inside this transaction
            onPreInsert(values, uri, db);
            id = db.insert(getTable(uri), null, values);

            if (useTransaction) {
                db.setTransactionSuccessful();
            }
        } catch (SQLiteBridgeException ex) {
            reportError(ex, TelemetryErrorOp.INSERT);
            throw ex;
        } finally {
            if (useTransaction) {
                db.endTransaction();
            }
        }

        return ContentUris.withAppendedId(uri, id);
    }

    @Override
    public int bulkInsert(Uri uri, ContentValues[] allValues) {
        final SQLiteBridge db = getDatabase(uri);
        // If we can not get a SQLiteBridge instance, its likely that the database
        // has not been set up and Gecko is not running. We return 0 and expect
        // callers to try again later
        if (db == null) {
            return 0;
        }

        int rowsAdded = 0;

        String table = getTable(uri);

        try {
            db.beginTransaction();
            for (ContentValues initialValues : allValues) {
                ContentValues values = new ContentValues(initialValues);
                setupDefaults(uri, values);
                onPreInsert(values, uri, db);
                db.insert(table, null, values);
                rowsAdded++;
            }
            db.setTransactionSuccessful();
        } catch (SQLiteBridgeException ex) {
            reportError(ex, TelemetryErrorOp.BULKINSERT);
            throw ex;
        } finally {
            db.endTransaction();
        }

        if (rowsAdded > 0) {
            final boolean shouldSyncToNetwork = !isCallerSync(uri);
            mContext.getContentResolver().notifyChange(uri, null, shouldSyncToNetwork);
        }

        return rowsAdded;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        int updated = 0;
        final SQLiteBridge db = getDatabase(uri);

        // If we can not get a SQLiteBridge instance, its likely that the database
        // has not been set up and Gecko is not running. We return null and expect
        // callers to try again later
        if (db == null) {
            return updated;
        }

        onPreUpdate(values, uri, db);

        try {
            updated = db.update(getTable(uri), values, selection, selectionArgs);
        } catch (SQLiteBridgeException ex) {
            reportError(ex, TelemetryErrorOp.UPDATE);
            throw ex;
        }

        return updated;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection,
            String[] selectionArgs, String sortOrder) {
        Cursor cursor = null;
        final SQLiteBridge db = getDatabase(uri);

        // If we can not get a SQLiteBridge instance, its likely that the database
        // has not been set up and Gecko is not running. We return null and expect
        // callers to try again later
        if (db == null) {
            return cursor;
        }

        sortOrder = getSortOrder(uri, sortOrder);

        try {
            cursor = db.query(getTable(uri), projection, selection, selectionArgs, null, null, sortOrder, null);
            onPostQuery(cursor, uri, db);
        } catch (SQLiteBridgeException ex) {
            reportError(ex, TelemetryErrorOp.QUERY);
            throw ex;
        }

        return cursor;
    }

    private String getHistogram(SQLiteBridgeException e) {
        // If you add values here, make sure to update
        // toolkit/components/telemetry/Histograms.json.
        if (ERROR_MESSAGE_DATABASE_IS_LOCKED.equals(e.getMessage())) {
            return getTelemetryPrefix() + "_LOCKED";
        }
        return null;
    }

    protected void reportError(SQLiteBridgeException e, TelemetryErrorOp op) {
        Log.e(mLogTag, "Error in database " + op.name(), e);
        final String histogram = getHistogram(e);
        if (histogram == null) {
            return;
        }

        Telemetry.HistogramAdd(histogram, op.getBucket());
    }

    protected abstract String getDBName();

    protected abstract int getDBVersion();

    protected abstract String getTable(Uri uri);

    protected abstract String getSortOrder(Uri uri, String aRequested);

    protected abstract void setupDefaults(Uri uri, ContentValues values);

    protected abstract void initGecko();

    protected abstract void onPreInsert(ContentValues values, Uri uri, SQLiteBridge db);

    protected abstract void onPreUpdate(ContentValues values, Uri uri, SQLiteBridge db);

    protected abstract void onPostQuery(Cursor cursor, Uri uri, SQLiteBridge db);
}
