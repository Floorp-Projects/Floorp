/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import android.content.ContentUris;
import android.content.ContentValues;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.DatabaseUtils;
import android.database.MatrixCursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteQueryBuilder;
import android.net.Uri;
import android.support.annotation.NonNull;
import android.text.TextUtils;
import android.util.Base64;

import org.mozilla.gecko.db.BrowserContract.DeletedLogins;
import org.mozilla.gecko.db.BrowserContract.Logins;
import org.mozilla.gecko.db.BrowserContract.LoginsDisabledHosts;
import org.mozilla.gecko.sync.Utils;

import java.io.UnsupportedEncodingException;
import java.security.GeneralSecurityException;
import java.util.HashMap;

import javax.crypto.Cipher;
import javax.crypto.NullCipher;

import static org.mozilla.gecko.db.BrowserContract.DeletedLogins.TABLE_DELETED_LOGINS;
import static org.mozilla.gecko.db.BrowserContract.Logins.TABLE_LOGINS;
import static org.mozilla.gecko.db.BrowserContract.LoginsDisabledHosts.TABLE_DISABLED_HOSTS;

public class LoginsProvider extends SharedBrowserDatabaseProvider {

    private static final int LOGINS = 100;
    private static final int LOGINS_ID = 101;
    private static final int DELETED_LOGINS = 102;
    private static final int DELETED_LOGINS_ID = 103;
    private static final int DISABLED_HOSTS = 104;
    private static final int DISABLED_HOSTS_HOSTNAME = 105;
    private static final UriMatcher URI_MATCHER = new UriMatcher(UriMatcher.NO_MATCH);

    private static final HashMap<String, String> LOGIN_PROJECTION_MAP;
    private static final HashMap<String, String> DELETED_LOGIN_PROJECTION_MAP;
    private static final HashMap<String, String> DISABLED_HOSTS_PROJECTION_MAP;

    private static final String DEFAULT_LOGINS_SORT_ORDER = Logins.HOSTNAME + " ASC";
    private static final String DEFAULT_DELETED_LOGINS_SORT_ORDER = DeletedLogins.TIME_DELETED + " ASC";
    private static final String DEFAULT_DISABLED_HOSTS_SORT_ORDER = LoginsDisabledHosts.HOSTNAME + " ASC";
    private static final String WHERE_GUID_IS_NULL = DeletedLogins.GUID + " IS NULL";
    private static final String WHERE_GUID_IS_VALUE = DeletedLogins.GUID + " = ?";

    protected static final String INDEX_LOGINS_HOSTNAME = "login_hostname_index";
    protected static final String INDEX_LOGINS_HOSTNAME_FORM_SUBMIT_URL = "login_hostname_formSubmitURL_index";
    protected static final String INDEX_LOGINS_HOSTNAME_HTTP_REALM = "login_hostname_httpRealm_index";

    static {
        URI_MATCHER.addURI(BrowserContract.LOGINS_AUTHORITY, "logins", LOGINS);
        URI_MATCHER.addURI(BrowserContract.LOGINS_AUTHORITY, "logins/#", LOGINS_ID);
        URI_MATCHER.addURI(BrowserContract.LOGINS_AUTHORITY, "deleted-logins", DELETED_LOGINS);
        URI_MATCHER.addURI(BrowserContract.LOGINS_AUTHORITY, "deleted-logins/#", DELETED_LOGINS_ID);
        URI_MATCHER.addURI(BrowserContract.LOGINS_AUTHORITY, "logins-disabled-hosts", DISABLED_HOSTS);
        URI_MATCHER.addURI(BrowserContract.LOGINS_AUTHORITY, "logins-disabled-hosts/hostname/*", DISABLED_HOSTS_HOSTNAME);

        LOGIN_PROJECTION_MAP = new HashMap<>();
        LOGIN_PROJECTION_MAP.put(Logins._ID, Logins._ID);
        LOGIN_PROJECTION_MAP.put(Logins.HOSTNAME, Logins.HOSTNAME);
        LOGIN_PROJECTION_MAP.put(Logins.HTTP_REALM, Logins.HTTP_REALM);
        LOGIN_PROJECTION_MAP.put(Logins.FORM_SUBMIT_URL, Logins.FORM_SUBMIT_URL);
        LOGIN_PROJECTION_MAP.put(Logins.USERNAME_FIELD, Logins.USERNAME_FIELD);
        LOGIN_PROJECTION_MAP.put(Logins.PASSWORD_FIELD, Logins.PASSWORD_FIELD);
        LOGIN_PROJECTION_MAP.put(Logins.ENCRYPTED_USERNAME, Logins.ENCRYPTED_USERNAME);
        LOGIN_PROJECTION_MAP.put(Logins.ENCRYPTED_PASSWORD, Logins.ENCRYPTED_PASSWORD);
        LOGIN_PROJECTION_MAP.put(Logins.GUID, Logins.GUID);
        LOGIN_PROJECTION_MAP.put(Logins.ENC_TYPE, Logins.ENC_TYPE);
        LOGIN_PROJECTION_MAP.put(Logins.TIME_CREATED, Logins.TIME_CREATED);
        LOGIN_PROJECTION_MAP.put(Logins.TIME_LAST_USED, Logins.TIME_LAST_USED);
        LOGIN_PROJECTION_MAP.put(Logins.TIME_PASSWORD_CHANGED, Logins.TIME_PASSWORD_CHANGED);
        LOGIN_PROJECTION_MAP.put(Logins.TIMES_USED, Logins.TIMES_USED);

        DELETED_LOGIN_PROJECTION_MAP = new HashMap<>();
        DELETED_LOGIN_PROJECTION_MAP.put(DeletedLogins._ID, DeletedLogins._ID);
        DELETED_LOGIN_PROJECTION_MAP.put(DeletedLogins.GUID, DeletedLogins.GUID);
        DELETED_LOGIN_PROJECTION_MAP.put(DeletedLogins.TIME_DELETED, DeletedLogins.TIME_DELETED);

        DISABLED_HOSTS_PROJECTION_MAP = new HashMap<>();
        DISABLED_HOSTS_PROJECTION_MAP.put(LoginsDisabledHosts._ID, LoginsDisabledHosts._ID);
        DISABLED_HOSTS_PROJECTION_MAP.put(LoginsDisabledHosts.HOSTNAME, LoginsDisabledHosts.HOSTNAME);
    }

    private static String projectColumn(String table, String column) {
        return table + "." + column;
    }

    private static String selectColumn(String table, String column) {
        return projectColumn(table, column) + " = ?";
    }

    @Override
    protected Uri insertInTransaction(Uri uri, ContentValues values) {
        trace("Calling insert in transaction on URI: " + uri);

        final int match = URI_MATCHER.match(uri);
        final SQLiteDatabase db = getWritableDatabase(uri);
        final long id;
        String guid;

        setupDefaultValues(values, uri);
        switch (match) {
            case LOGINS:
                removeDeletedLoginsByGUIDInTransaction(values, db);
                // Encrypt sensitive data.
                encryptContentValueFields(values);
                guid = values.getAsString(Logins.GUID);
                debug("Inserting login in database with GUID: " + guid);
                id = db.insertOrThrow(TABLE_LOGINS, Logins.GUID, values);
                break;

            case DELETED_LOGINS:
                guid = values.getAsString(DeletedLogins.GUID);
                debug("Inserting deleted-login in database with GUID: " + guid);
                id = db.insertOrThrow(TABLE_DELETED_LOGINS, DeletedLogins.GUID, values);
                break;

            case DISABLED_HOSTS:
                String hostname = values.getAsString(LoginsDisabledHosts.HOSTNAME);
                debug("Inserting disabled-host in database with hostname: " + hostname);
                id = db.insertOrThrow(TABLE_DISABLED_HOSTS, LoginsDisabledHosts.HOSTNAME, values);
                break;

            default:
                throw new UnsupportedOperationException("Unknown insert URI " + uri);
        }

        debug("Inserted ID in database: " + id);

        if (id >= 0) {
            return ContentUris.withAppendedId(uri, id);
        }

        return null;
    }

    @Override
    @SuppressWarnings("fallthrough")
    protected int deleteInTransaction(Uri uri, String selection, String[] selectionArgs) {
        trace("Calling delete in transaction on URI: " + uri);

        final int match = URI_MATCHER.match(uri);
        final String table;
        final SQLiteDatabase db = getWritableDatabase(uri);

        beginWrite(db);
        switch (match) {
            case LOGINS_ID:
                trace("Delete on LOGINS_ID: " + uri);
                selection = DBUtils.concatenateWhere(selection, selectColumn(TABLE_LOGINS, Logins._ID));
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[]{Long.toString(ContentUris.parseId(uri))});
                // Store the deleted client in deleted-logins table.
                final String guid = getLoginGUIDByID(selection, selectionArgs, db);
                if (guid == null) {
                    // No matching logins found for the id.
                    return 0;
                }
                boolean isInsertSuccessful = storeDeletedLoginForGUIDInTransaction(guid, db);
                if (!isInsertSuccessful) {
                    // Failed to insert into deleted-logins, return early.
                    return 0;
                }
            // fall through
            case LOGINS:
                trace("Delete on LOGINS: " + uri);
                table = TABLE_LOGINS;
                break;

            case DELETED_LOGINS_ID:
                trace("Delete on DELETED_LOGINS_ID: " + uri);
                selection = DBUtils.concatenateWhere(selection, selectColumn(TABLE_DELETED_LOGINS, DeletedLogins._ID));
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[]{Long.toString(ContentUris.parseId(uri))});
            // fall through
            case DELETED_LOGINS:
                trace("Delete on DELETED_LOGINS_ID: " + uri);
                table = TABLE_DELETED_LOGINS;
                break;

            case DISABLED_HOSTS_HOSTNAME:
                trace("Delete on DISABLED_HOSTS_HOSTNAME: " + uri);
                selection = DBUtils.concatenateWhere(selection, selectColumn(TABLE_DISABLED_HOSTS, LoginsDisabledHosts.HOSTNAME));
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[]{uri.getLastPathSegment()});
            // fall through
            case DISABLED_HOSTS:
                trace("Delete on DISABLED_HOSTS: " + uri);
                table = TABLE_DISABLED_HOSTS;
                break;

            default:
                throw new UnsupportedOperationException("Unknown delete URI " + uri);
        }

        debug("Deleting " + table + " for URI: " + uri);
        return db.delete(table, selection, selectionArgs);
    }

    @Override
    @SuppressWarnings("fallthrough")
    protected int updateInTransaction(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        trace("Calling update in transaction on URI: " + uri);

        final int match = URI_MATCHER.match(uri);
        final SQLiteDatabase db = getWritableDatabase(uri);
        final String table;

        beginWrite(db);
        switch (match) {
            case LOGINS_ID:
                trace("Update on LOGINS_ID: " + uri);
                selection = DBUtils.concatenateWhere(selection, selectColumn(TABLE_LOGINS, Logins._ID));
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[]{Long.toString(ContentUris.parseId(uri))});

            case LOGINS:
                trace("Update on LOGINS: " + uri);
                table = TABLE_LOGINS;
                // Encrypt sensitive data.
                encryptContentValueFields(values);
                break;

            default:
                throw new UnsupportedOperationException("Unknown update URI " + uri);
        }

        trace("Updating " + table + " on URI: " + uri);
        return db.update(table, values, selection, selectionArgs);

    }

    @Override
    @SuppressWarnings("fallthrough")
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
        trace("Calling query on URI: " + uri);

        final SQLiteDatabase db = getReadableDatabase(uri);
        final int match = URI_MATCHER.match(uri);
        final String groupBy = null;
        final SQLiteQueryBuilder qb = new SQLiteQueryBuilder();
        final String limit = uri.getQueryParameter(BrowserContract.PARAM_LIMIT);

        switch (match) {
            case LOGINS_ID:
                trace("Query is on LOGINS_ID: " + uri);
                selection = DBUtils.concatenateWhere(selection, selectColumn(TABLE_LOGINS, Logins._ID));
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });

            // fall through
            case LOGINS:
                trace("Query is on LOGINS: " + uri);
                if (TextUtils.isEmpty(sortOrder)) {
                    sortOrder = DEFAULT_LOGINS_SORT_ORDER;
                } else {
                    debug("Using sort order " + sortOrder + ".");
                }

                qb.setProjectionMap(LOGIN_PROJECTION_MAP);
                qb.setTables(TABLE_LOGINS);
                break;

            case DELETED_LOGINS_ID:
                trace("Query is on DELETED_LOGINS_ID: " + uri);
                selection = DBUtils.concatenateWhere(selection, selectColumn(TABLE_DELETED_LOGINS, DeletedLogins._ID));
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });

            // fall through
            case DELETED_LOGINS:
                trace("Query is on DELETED_LOGINS: " + uri);
                if (TextUtils.isEmpty(sortOrder)) {
                    sortOrder = DEFAULT_DELETED_LOGINS_SORT_ORDER;
                } else {
                    debug("Using sort order " + sortOrder + ".");
                }

                qb.setProjectionMap(DELETED_LOGIN_PROJECTION_MAP);
                qb.setTables(TABLE_DELETED_LOGINS);
                break;

            case DISABLED_HOSTS_HOSTNAME:
                trace("Query is on DISABLED_HOSTS_HOSTNAME: " + uri);
                selection = DBUtils.concatenateWhere(selection, selectColumn(TABLE_DISABLED_HOSTS, LoginsDisabledHosts.HOSTNAME));
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { uri.getLastPathSegment() });

            // fall through
            case DISABLED_HOSTS:
                trace("Query is on DISABLED_HOSTS: " + uri);
                if (TextUtils.isEmpty(sortOrder)) {
                    sortOrder = DEFAULT_DISABLED_HOSTS_SORT_ORDER;
                } else {
                    debug("Using sort order " + sortOrder + ".");
                }

                qb.setProjectionMap(DISABLED_HOSTS_PROJECTION_MAP);
                qb.setTables(TABLE_DISABLED_HOSTS);
                break;

            default:
                throw new UnsupportedOperationException("Unknown query URI " + uri);
        }

        trace("Running built query.");
        Cursor cursor = qb.query(db, projection, selection, selectionArgs, groupBy, null, sortOrder, limit);
        // If decryptManyCursorRows does not return the original cursor, it closes it, so there's
        // no need to close here.
        cursor = decryptManyCursorRows(cursor);
        cursor.setNotificationUri(getContext().getContentResolver(), BrowserContract.LOGINS_AUTHORITY_URI);
        return cursor;
    }

    @Override
    public String getType(@NonNull Uri uri) {
        final int match = URI_MATCHER.match(uri);

        switch (match) {
            case LOGINS:
                return Logins.CONTENT_TYPE;

            case LOGINS_ID:
                return Logins.CONTENT_ITEM_TYPE;

            case DELETED_LOGINS:
                return DeletedLogins.CONTENT_TYPE;

            case DELETED_LOGINS_ID:
                return DeletedLogins.CONTENT_ITEM_TYPE;

            case DISABLED_HOSTS:
                return LoginsDisabledHosts.CONTENT_TYPE;

            case DISABLED_HOSTS_HOSTNAME:
                return LoginsDisabledHosts.CONTENT_ITEM_TYPE;

            default:
                throw new UnsupportedOperationException("Unknown type " + uri);
        }
    }

    /**
     * Caller is responsible for invoking this method inside a transaction.
     */
    private String getLoginGUIDByID(final String selection, final String[] selectionArgs, final SQLiteDatabase db) {
        final Cursor cursor = db.query(Logins.TABLE_LOGINS, new String[]{Logins.GUID}, selection, selectionArgs, null, null, DEFAULT_LOGINS_SORT_ORDER);
        try {
            if (!cursor.moveToFirst()) {
                return null;
            }
            return cursor.getString(cursor.getColumnIndexOrThrow(Logins.GUID));
        } finally {
            cursor.close();
        }
    }

    /**
     * Caller is responsible for invoking this method inside a transaction.
     */
    private boolean storeDeletedLoginForGUIDInTransaction(final String guid, final SQLiteDatabase db) {
        if (guid == null) {
            return false;
        }
        final ContentValues values = new ContentValues();
        values.put(DeletedLogins.GUID, guid);
        values.put(DeletedLogins.TIME_DELETED, System.currentTimeMillis());
        return db.insert(TABLE_DELETED_LOGINS, DeletedLogins.GUID, values) > 0;
    }

    /**
     * Caller is responsible for invoking this method inside a transaction.
     */
    private void removeDeletedLoginsByGUIDInTransaction(ContentValues values, SQLiteDatabase db) {
        if (values.containsKey(Logins.GUID)) {
            final String guid = values.getAsString(Logins.GUID);
            if (guid == null) {
                db.delete(TABLE_DELETED_LOGINS, WHERE_GUID_IS_NULL, null);
            } else {
                String[] args = new String[]{guid};
                db.delete(TABLE_DELETED_LOGINS, WHERE_GUID_IS_VALUE, args);
            }
        }
    }

    private void setupDefaultValues(ContentValues values, Uri uri) throws IllegalArgumentException {
        final int match = URI_MATCHER.match(uri);
        final long now = System.currentTimeMillis();
        switch (match) {
            case DELETED_LOGINS:
                values.put(DeletedLogins.TIME_DELETED, now);
                // deleted-logins must contain a guid
                if (!values.containsKey(DeletedLogins.GUID)) {
                    throw new IllegalArgumentException("Must provide GUID for deleted-login");
                }
                break;

            case LOGINS:
                values.put(Logins.TIME_CREATED, now);
                // Generate GUID for new login. Don't override specified GUIDs.
                if (!values.containsKey(Logins.GUID)) {
                    final String guid = Utils.generateGuid();
                    values.put(Logins.GUID, guid);
                }
                // The database happily accepts strings for long values; this just lets us re-use
                // the existing helper method.
                String nowString = Long.toString(now);
                DBUtils.replaceKey(values, null, Logins.HTTP_REALM, null);
                DBUtils.replaceKey(values, null, Logins.FORM_SUBMIT_URL, null);
                DBUtils.replaceKey(values, null, Logins.ENC_TYPE, "0");
                DBUtils.replaceKey(values, null, Logins.TIME_LAST_USED, nowString);
                DBUtils.replaceKey(values, null, Logins.TIME_PASSWORD_CHANGED, nowString);
                DBUtils.replaceKey(values, null, Logins.TIMES_USED, "0");
                break;

            case DISABLED_HOSTS:
                if (!values.containsKey(LoginsDisabledHosts.HOSTNAME)) {
                    throw new IllegalArgumentException("Must provide hostname for disabled-host");
                }
                break;

            default:
                throw new UnsupportedOperationException("Unknown URI in setupDefaultValues " + uri);
        }
    }

    private void encryptContentValueFields(final ContentValues values) {
        if (values.containsKey(Logins.ENCRYPTED_PASSWORD)) {
            final String res = encrypt(values.getAsString(Logins.ENCRYPTED_PASSWORD));
            values.put(Logins.ENCRYPTED_PASSWORD, res);
        }

        if (values.containsKey(Logins.ENCRYPTED_USERNAME)) {
            final String res = encrypt(values.getAsString(Logins.ENCRYPTED_USERNAME));
            values.put(Logins.ENCRYPTED_USERNAME, res);
        }
    }

    /**
     * Replace each password and username encrypted ciphertext with its equivalent decrypted
     * plaintext in the given cursor.
     * <p/>
     * The encryption algorithm used to protect logins is unspecified; and further, a consumer of
     * consumers should never have access to encrypted ciphertext.
     *
     * @param cursor containing at least one of password and username encrypted ciphertexts.
     * @return a new {@link Cursor} with password and username decrypted plaintexts.
     */
    private Cursor decryptManyCursorRows(final Cursor cursor) {
        final int passwordIndex = cursor.getColumnIndex(Logins.ENCRYPTED_PASSWORD);
        final int usernameIndex = cursor.getColumnIndex(Logins.ENCRYPTED_USERNAME);

        if (passwordIndex == -1 && usernameIndex == -1) {
            return cursor;
        }

        // Special case, decrypt the encrypted username or password before returning the cursor.
        final MatrixCursor newCursor = new MatrixCursor(cursor.getColumnNames(), cursor.getColumnCount());
        try {
            for (cursor.moveToFirst(); !cursor.isAfterLast(); cursor.moveToNext()) {
                final ContentValues values = new ContentValues();
                DatabaseUtils.cursorRowToContentValues(cursor, values);

                if (passwordIndex > -1) {
                    String decrypted = decrypt(values.getAsString(Logins.ENCRYPTED_PASSWORD));
                    values.put(Logins.ENCRYPTED_PASSWORD, decrypted);
                }

                if (usernameIndex > -1) {
                    String decrypted = decrypt(values.getAsString(Logins.ENCRYPTED_USERNAME));
                    values.put(Logins.ENCRYPTED_USERNAME, decrypted);
                }

                final MatrixCursor.RowBuilder rowBuilder = newCursor.newRow();
                for (String key : cursor.getColumnNames()) {
                    rowBuilder.add(values.get(key));
                }
            }
        } finally {
            // Close the old cursor before returning the new one.
            cursor.close();
        }

        return newCursor;
    }

    private String encrypt(@NonNull String initialValue) {
        try {
            final Cipher cipher = getCipher(Cipher.ENCRYPT_MODE);
            return Base64.encodeToString(cipher.doFinal(initialValue.getBytes("UTF-8")), Base64.URL_SAFE);
        } catch (Exception e) {
            debug("encryption failed : " + e);
            throw new IllegalStateException("Logins encryption failed", e);
        }
    }

    private String decrypt(@NonNull String initialValue) {
        try {
            final Cipher cipher = getCipher(Cipher.DECRYPT_MODE);
            return new String(cipher.doFinal(Base64.decode(initialValue.getBytes("UTF-8"), Base64.URL_SAFE)));
        } catch (Exception e) {
            debug("Decryption failed : " + e);
            throw new IllegalStateException("Logins decryption failed", e);
        }
    }

    private Cipher getCipher(int mode) throws UnsupportedEncodingException, GeneralSecurityException {
        return new NullCipher();
    }
}
