/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import java.util.HashMap;

import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.NSSBridge;
import org.mozilla.gecko.db.BrowserContract.DeletedPasswords;
import org.mozilla.gecko.db.BrowserContract.Passwords;
import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.sqlite.MatrixBlobCursor;
import org.mozilla.gecko.sqlite.SQLiteBridge;
import org.mozilla.gecko.sync.Utils;

import android.content.ContentValues;
import android.content.Intent;
import android.content.UriMatcher;
import android.database.Cursor;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Log;

public class PasswordsProvider extends SQLiteBridgeContentProvider {
    static final String TABLE_PASSWORDS = "moz_logins";
    static final String TABLE_DELETED_PASSWORDS = "moz_deleted_logins";

    private static final String TELEMETRY_TAG = "SQLITEBRIDGE_PROVIDER_PASSWORDS";

    private static final int PASSWORDS = 100;
    private static final int DELETED_PASSWORDS = 101;

    static final String DEFAULT_PASSWORDS_SORT_ORDER = Passwords.HOSTNAME + " ASC";
    static final String DEFAULT_DELETED_PASSWORDS_SORT_ORDER = DeletedPasswords.TIME_DELETED + " ASC";

    private static final UriMatcher URI_MATCHER;

    private static HashMap<String, String> PASSWORDS_PROJECTION_MAP;
    private static HashMap<String, String> DELETED_PASSWORDS_PROJECTION_MAP;

    // this should be kept in sync with the version in toolkit/components/passwordmgr/storage-mozStorage.js
    private static final int DB_VERSION = 5;
    private static final String DB_FILENAME = "signons.sqlite";
    private static final String WHERE_GUID_IS_NULL = BrowserContract.DeletedPasswords.GUID + " IS NULL";
    private static final String WHERE_GUID_IS_VALUE = BrowserContract.DeletedPasswords.GUID + " = ?";

    private static final String LOG_TAG = "GeckPasswordsProvider";

    static {
        URI_MATCHER = new UriMatcher(UriMatcher.NO_MATCH);

        // content://org.mozilla.gecko.providers.browser/passwords/#
        URI_MATCHER.addURI(BrowserContract.PASSWORDS_AUTHORITY, "passwords", PASSWORDS);

        PASSWORDS_PROJECTION_MAP = new HashMap<String, String>();
        PASSWORDS_PROJECTION_MAP.put(Passwords.ID, Passwords.ID);
        PASSWORDS_PROJECTION_MAP.put(Passwords.HOSTNAME, Passwords.HOSTNAME);
        PASSWORDS_PROJECTION_MAP.put(Passwords.HTTP_REALM, Passwords.HTTP_REALM);
        PASSWORDS_PROJECTION_MAP.put(Passwords.FORM_SUBMIT_URL, Passwords.FORM_SUBMIT_URL);
        PASSWORDS_PROJECTION_MAP.put(Passwords.USERNAME_FIELD, Passwords.USERNAME_FIELD);
        PASSWORDS_PROJECTION_MAP.put(Passwords.PASSWORD_FIELD, Passwords.PASSWORD_FIELD);
        PASSWORDS_PROJECTION_MAP.put(Passwords.ENCRYPTED_USERNAME, Passwords.ENCRYPTED_USERNAME);
        PASSWORDS_PROJECTION_MAP.put(Passwords.ENCRYPTED_PASSWORD, Passwords.ENCRYPTED_PASSWORD);
        PASSWORDS_PROJECTION_MAP.put(Passwords.GUID, Passwords.GUID);
        PASSWORDS_PROJECTION_MAP.put(Passwords.ENC_TYPE, Passwords.ENC_TYPE);
        PASSWORDS_PROJECTION_MAP.put(Passwords.TIME_CREATED, Passwords.TIME_CREATED);
        PASSWORDS_PROJECTION_MAP.put(Passwords.TIME_LAST_USED, Passwords.TIME_LAST_USED);
        PASSWORDS_PROJECTION_MAP.put(Passwords.TIME_PASSWORD_CHANGED, Passwords.TIME_PASSWORD_CHANGED);
        PASSWORDS_PROJECTION_MAP.put(Passwords.TIMES_USED, Passwords.TIMES_USED);

        URI_MATCHER.addURI(BrowserContract.PASSWORDS_AUTHORITY, "deleted-passwords", DELETED_PASSWORDS);

        DELETED_PASSWORDS_PROJECTION_MAP = new HashMap<String, String>();
        DELETED_PASSWORDS_PROJECTION_MAP.put(DeletedPasswords.ID, DeletedPasswords.ID);
        DELETED_PASSWORDS_PROJECTION_MAP.put(DeletedPasswords.GUID, DeletedPasswords.GUID);
        DELETED_PASSWORDS_PROJECTION_MAP.put(DeletedPasswords.TIME_DELETED, DeletedPasswords.TIME_DELETED);
    }

    public PasswordsProvider() {
        super(LOG_TAG);

        // We don't use .loadMozGlue because we're in a different process,
        // and we just want to reuse code rather than use the loader lock etc.
        GeckoLoader.doLoadLibrary(getContext(), "mozglue");
    }

    @Override
    protected String getDBName(){
        return DB_FILENAME;
    }

    @Override
    protected String getTelemetryPrefix() {
        return TELEMETRY_TAG;
    }

    @Override
    protected int getDBVersion(){
        return DB_VERSION;
    }

    @Override
    public String getType(Uri uri) {
        final int match = URI_MATCHER.match(uri);

        switch (match) {
            case PASSWORDS:
                return Passwords.CONTENT_TYPE;

            case DELETED_PASSWORDS:
                return DeletedPasswords.CONTENT_TYPE;

            default:
                throw new UnsupportedOperationException("Unknown type " + uri);
        }
    }

    @Override
    public String getTable(Uri uri) {
        final int match = URI_MATCHER.match(uri);
        switch (match) {
            case DELETED_PASSWORDS:
                return TABLE_DELETED_PASSWORDS;

            case PASSWORDS:
                return TABLE_PASSWORDS;

            default:
                throw new UnsupportedOperationException("Unknown table " + uri);
        }
    }

    @Override
    public String getSortOrder(Uri uri, String aRequested) {
        if (!TextUtils.isEmpty(aRequested)) {
            return aRequested;
        }

        final int match = URI_MATCHER.match(uri);
        switch (match) {
            case DELETED_PASSWORDS:
                return DEFAULT_DELETED_PASSWORDS_SORT_ORDER;

            case PASSWORDS:
                return DEFAULT_PASSWORDS_SORT_ORDER;

            default:
                throw new UnsupportedOperationException("Unknown URI " + uri);
        }
    }

    @Override
    public void setupDefaults(Uri uri, ContentValues values)
            throws IllegalArgumentException {
        int match = URI_MATCHER.match(uri);
        long now = System.currentTimeMillis();
        switch (match) {
            case DELETED_PASSWORDS:
                values.put(DeletedPasswords.TIME_DELETED, now);

                // Deleted passwords must contain a guid
                if (!values.containsKey(Passwords.GUID)) {
                    throw new IllegalArgumentException("Must provide a GUID for a deleted password");
                }
                break;

            case PASSWORDS:
                values.put(Passwords.TIME_CREATED, now);

                // Generate GUID for new password. Don't override specified GUIDs.
                if (!values.containsKey(Passwords.GUID)) {
                    String guid = Utils.generateGuid();
                    values.put(Passwords.GUID, guid);
                }
                String nowString = Long.toString(now);
                DBUtils.replaceKey(values, null, Passwords.HOSTNAME, "");
                DBUtils.replaceKey(values, null, Passwords.HTTP_REALM, "");
                DBUtils.replaceKey(values, null, Passwords.FORM_SUBMIT_URL, "");
                DBUtils.replaceKey(values, null, Passwords.USERNAME_FIELD, "");
                DBUtils.replaceKey(values, null, Passwords.PASSWORD_FIELD, "");
                DBUtils.replaceKey(values, null, Passwords.ENCRYPTED_USERNAME, "");
                DBUtils.replaceKey(values, null, Passwords.ENCRYPTED_PASSWORD, "");
                DBUtils.replaceKey(values, null, Passwords.ENC_TYPE, "0");
                DBUtils.replaceKey(values, null, Passwords.TIME_LAST_USED, nowString);
                DBUtils.replaceKey(values, null, Passwords.TIME_PASSWORD_CHANGED, nowString);
                DBUtils.replaceKey(values, null, Passwords.TIMES_USED, "0");
                break;

            default:
                throw new UnsupportedOperationException("Unknown URI " + uri);
        }
    }

    @Override
    public void initGecko() {
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Passwords:Init", null));
        Intent initIntent = new Intent(GeckoApp.ACTION_INIT_PW);
        mContext.sendBroadcast(initIntent);
    }

    private String doCrypto(String initialValue, Uri uri, Boolean encrypt) {
        String profilePath = null;
        if (uri != null) {
            profilePath = uri.getQueryParameter(BrowserContract.PARAM_PROFILE_PATH);
        }

        String result = "";
        try {
            if (encrypt) {
                if (profilePath != null) {
                    result = NSSBridge.encrypt(mContext, profilePath, initialValue);
                } else {
                    result = NSSBridge.encrypt(mContext, initialValue);
                }
            } else {
                if (profilePath != null) {
                    result = NSSBridge.decrypt(mContext, profilePath, initialValue);
                } else {
                    result = NSSBridge.decrypt(mContext, initialValue);
                }
            }
        } catch (Exception ex) {
            Log.e(LOG_TAG, "Error in NSSBridge");
            throw new RuntimeException(ex);
        }
        return result;
    }

    @Override
    public void onPreInsert(ContentValues values, Uri uri, SQLiteBridge db) {
        if (values.containsKey(Passwords.GUID)) {
            String guid = values.getAsString(Passwords.GUID);
            if (guid == null) {
                db.delete(TABLE_DELETED_PASSWORDS, WHERE_GUID_IS_NULL, null);
                return;
            }
            String[] args = new String[] { guid };
            db.delete(TABLE_DELETED_PASSWORDS, WHERE_GUID_IS_VALUE, args);
        }

        if (values.containsKey(Passwords.ENCRYPTED_PASSWORD)) {
            String res = doCrypto(values.getAsString(Passwords.ENCRYPTED_PASSWORD), uri, true);
            values.put(Passwords.ENCRYPTED_PASSWORD, res);
            values.put(Passwords.ENC_TYPE, Passwords.ENCTYPE_SDR);
        }

        if (values.containsKey(Passwords.ENCRYPTED_USERNAME)) {
            String res = doCrypto(values.getAsString(Passwords.ENCRYPTED_USERNAME), uri, true);
            values.put(Passwords.ENCRYPTED_USERNAME, res);
            values.put(Passwords.ENC_TYPE, Passwords.ENCTYPE_SDR);
        }
    }

    @Override
    public void onPreUpdate(ContentValues values, Uri uri, SQLiteBridge db) {
        if (values.containsKey(Passwords.ENCRYPTED_PASSWORD)) {
            String res = doCrypto(values.getAsString(Passwords.ENCRYPTED_PASSWORD), uri, true);
            values.put(Passwords.ENCRYPTED_PASSWORD, res);
            values.put(Passwords.ENC_TYPE, Passwords.ENCTYPE_SDR);
        }

        if (values.containsKey(Passwords.ENCRYPTED_USERNAME)) {
            String res = doCrypto(values.getAsString(Passwords.ENCRYPTED_USERNAME), uri, true);
            values.put(Passwords.ENCRYPTED_USERNAME, res);
            values.put(Passwords.ENC_TYPE, Passwords.ENCTYPE_SDR);
        }
    }

    @Override
    public void onPostQuery(Cursor cursor, Uri uri, SQLiteBridge db) {
        int passwordIndex = -1;
        int usernameIndex = -1;
        String profilePath = null;

        try {
            passwordIndex = cursor.getColumnIndexOrThrow(Passwords.ENCRYPTED_PASSWORD);
        } catch(Exception ex) { }
        try {
            usernameIndex = cursor.getColumnIndexOrThrow(Passwords.ENCRYPTED_USERNAME);
        } catch(Exception ex) { }

        if (passwordIndex > -1 || usernameIndex > -1) {
            MatrixBlobCursor m = (MatrixBlobCursor)cursor;
            if (cursor.moveToFirst()) {
                do {
                    if (passwordIndex > -1) {
                        String decrypted = doCrypto(cursor.getString(passwordIndex), uri, false);;
                        m.set(passwordIndex, decrypted);
                    }

                    if (usernameIndex > -1) {
                        String decrypted = doCrypto(cursor.getString(usernameIndex), uri, false);
                        m.set(usernameIndex, decrypted);
                    }
                } while(cursor.moveToNext());
            }
        }
    }
}
