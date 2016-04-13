/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import java.lang.IllegalArgumentException;
import java.util.HashMap;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.db.BrowserContract.FormHistory;
import org.mozilla.gecko.db.BrowserContract.DeletedFormHistory;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sqlite.SQLiteBridge;
import org.mozilla.gecko.sync.Utils;

import android.content.ContentValues;
import android.content.UriMatcher;
import android.database.Cursor;
import android.net.Uri;
import android.text.TextUtils;

public class FormHistoryProvider extends SQLiteBridgeContentProvider {
    static final String TABLE_FORM_HISTORY = "moz_formhistory";
    static final String TABLE_DELETED_FORM_HISTORY = "moz_deleted_formhistory";

    private static final int FORM_HISTORY = 100;
    private static final int DELETED_FORM_HISTORY = 101;

    private static final UriMatcher URI_MATCHER;


    // This should be kept in sync with the db version in toolkit/components/satchel/nsFormHistory.js
    private static final int DB_VERSION = 4;
    private static final String DB_FILENAME = "formhistory.sqlite";
    private static final String TELEMETRY_TAG = "SQLITEBRIDGE_PROVIDER_FORMS";

    private static final String WHERE_GUID_IS_NULL = BrowserContract.DeletedFormHistory.GUID + " IS NULL";
    private static final String WHERE_GUID_IS_VALUE = BrowserContract.DeletedFormHistory.GUID + " = ?";

    private static final String LOG_TAG = "FormHistoryProvider";

    static {
        URI_MATCHER = new UriMatcher(UriMatcher.NO_MATCH);
        URI_MATCHER.addURI(BrowserContract.FORM_HISTORY_AUTHORITY, "formhistory", FORM_HISTORY);
        URI_MATCHER.addURI(BrowserContract.FORM_HISTORY_AUTHORITY, "deleted-formhistory", DELETED_FORM_HISTORY);
    }

    public FormHistoryProvider() {
        super(LOG_TAG);
    }


    @Override
    public String getType(Uri uri) {
        final int match = URI_MATCHER.match(uri);

        switch (match) {
            case FORM_HISTORY:
                return FormHistory.CONTENT_TYPE;

            case DELETED_FORM_HISTORY:
                return DeletedFormHistory.CONTENT_TYPE;

            default:
                throw new UnsupportedOperationException("Unknown type " + uri);
        }
    }

    @Override
    public String getTable(Uri uri) {
        String table = null;
        final int match = URI_MATCHER.match(uri);
        switch (match) {
            case DELETED_FORM_HISTORY:
                table = TABLE_DELETED_FORM_HISTORY;
                break;

            case FORM_HISTORY:
                table = TABLE_FORM_HISTORY;
                break;

            default:
                throw new UnsupportedOperationException("Unknown table " + uri);
        }
        return table;
    }

    @Override
    public String getSortOrder(Uri uri, String aRequested) {
        if (!TextUtils.isEmpty(aRequested)) {
            return aRequested;
        }

        return null;
    }

    @Override
    public void setupDefaults(Uri uri, ContentValues values) {
        int match = URI_MATCHER.match(uri);
        long now = System.currentTimeMillis();

        switch (match) {
            case DELETED_FORM_HISTORY:
                values.put(DeletedFormHistory.TIME_DELETED, now);

                // Deleted entries must contain a guid
                if (!values.containsKey(FormHistory.GUID)) {
                    throw new IllegalArgumentException("Must provide a GUID for a deleted form history");
                }
                break;

            case FORM_HISTORY:
                // Generate GUID for new entry. Don't override specified GUIDs.
                if (!values.containsKey(FormHistory.GUID)) {
                    String guid = Utils.generateGuid();
                    values.put(FormHistory.GUID, guid);
                }
                break;

            default:
                throw new UnsupportedOperationException("Unknown insert URI " + uri);
        }
    }

    @Override
    public void initGecko() {
        GeckoAppShell.notifyObservers("FormHistory:Init", null);
    }

    @Override
    public void onPreInsert(ContentValues values, Uri uri, SQLiteBridge db) {
        if (!values.containsKey(FormHistory.GUID)) {
            return;
        }

        String guid = values.getAsString(FormHistory.GUID);
        if (guid == null) {
            db.delete(TABLE_DELETED_FORM_HISTORY, WHERE_GUID_IS_NULL, null);
            return;
        }
        String[] args = new String[] { guid };
        db.delete(TABLE_DELETED_FORM_HISTORY, WHERE_GUID_IS_VALUE, args);
     }

    @Override
    public void onPreUpdate(ContentValues values, Uri uri, SQLiteBridge db) { }

    @Override
    public void onPostQuery(Cursor cursor, Uri uri, SQLiteBridge db) { }

    @Override
    protected String getDBName() {
        return DB_FILENAME;
    }

    @Override
    protected String getTelemetryPrefix() {
        return TELEMETRY_TAG;
    }

    @Override
    protected int getDBVersion() {
        return DB_VERSION;
    }
}
