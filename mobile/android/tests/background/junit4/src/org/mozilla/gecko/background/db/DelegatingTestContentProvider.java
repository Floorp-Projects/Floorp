/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.db;

import android.content.ContentProvider;
import android.content.ContentProviderOperation;
import android.content.ContentProviderResult;
import android.content.ContentValues;
import android.content.OperationApplicationException;
import android.database.Cursor;
import android.net.Uri;

import org.mozilla.gecko.db.BrowserContract;

import java.util.ArrayList;

/**
 * Wrap a ContentProvider, appending &test=1 to all queries.
 */
public class DelegatingTestContentProvider extends ContentProvider {
    protected final ContentProvider mTargetProvider;

    protected static Uri appendUriParam(Uri uri, String param, String value) {
        return uri.buildUpon().appendQueryParameter(param, value).build();
    }

    public DelegatingTestContentProvider(ContentProvider targetProvider) {
        super();
        mTargetProvider = targetProvider;
    }

    private Uri appendTestParam(Uri uri) {
        return appendUriParam(uri, BrowserContract.PARAM_IS_TEST, "1");
    }

    @Override
    public boolean onCreate() {
        return mTargetProvider.onCreate();
    }

    @Override
    public String getType(Uri uri) {
        return mTargetProvider.getType(uri);
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return mTargetProvider.delete(appendTestParam(uri), selection, selectionArgs);
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return mTargetProvider.insert(appendTestParam(uri), values);
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection,
                      String[] selectionArgs) {
        return mTargetProvider.update(appendTestParam(uri), values,
                selection, selectionArgs);
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection,
                        String[] selectionArgs, String sortOrder) {
        return mTargetProvider.query(appendTestParam(uri), projection, selection,
                selectionArgs, sortOrder);
    }

    @Override
    public ContentProviderResult[] applyBatch(ArrayList<ContentProviderOperation> operations)
            throws OperationApplicationException {
        return mTargetProvider.applyBatch(operations);
    }

    @Override
    public int bulkInsert(Uri uri, ContentValues[] values) {
        return mTargetProvider.bulkInsert(appendTestParam(uri), values);
    }

    public ContentProvider getTargetProvider() {
        return mTargetProvider;
    }
}
