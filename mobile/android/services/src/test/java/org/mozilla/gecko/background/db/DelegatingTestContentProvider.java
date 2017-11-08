/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.db;

import android.content.ContentProvider;
import android.content.ContentProviderOperation;
import android.content.ContentProviderResult;
import android.content.ContentValues;
import android.content.Context;
import android.content.OperationApplicationException;
import android.content.pm.ProviderInfo;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.Nullable;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserProvider;
import org.mozilla.gecko.db.TabsProvider;
import org.robolectric.android.controller.ContentProviderController;
import org.robolectric.util.ReflectionHelpers;

import java.util.ArrayList;

/**
 * Wrap a ContentProvider, appending &test=1 to all queries.
 */
public class DelegatingTestContentProvider extends ContentProvider {
    protected ContentProvider mTargetProvider;

    protected static Uri appendUriParam(Uri uri, String param, String value) {
        return uri.buildUpon().appendQueryParameter(param, value).build();
    }

    /**
     * Create and register a new <tt>BrowserProvider</tt> that has test delegation.
     * <p>
     * Robolectric doesn't make it easy to parameterize a created
     * <tt>ContentProvider</tt>, so we modify a built-in helper to do it.
     * @return delegated <tt>ContentProvider</tt>.
     */
    public static ContentProvider createDelegatingBrowserProvider() {
        final ContentProviderController<DelegatingTestContentProvider> contentProviderController
                = ContentProviderController.of(ReflectionHelpers.callConstructor(DelegatingTestContentProvider.class,
                ReflectionHelpers.ClassParameter.from(ContentProvider.class, new BrowserProvider())));
        return contentProviderController.create(BrowserContract.AUTHORITY).get();
    }

    /**
     * Create and register a new <tt>TabsProvider</tt> that has test delegation.
     * <p>
     * Robolectric doesn't make it easy to parameterize a created
     * <tt>ContentProvider</tt>, so we modify a built-in helper to do it.
     * @return delegated <tt>ContentProvider</tt>.
     */
    public static ContentProvider createDelegatingTabsProvider() {
        final ContentProviderController<DelegatingTestContentProvider> contentProviderController
                = ContentProviderController.of(ReflectionHelpers.callConstructor(DelegatingTestContentProvider.class,
                ReflectionHelpers.ClassParameter.from(ContentProvider.class, new TabsProvider())));
        return contentProviderController.create(BrowserContract.TABS_AUTHORITY).get();
    }

    public DelegatingTestContentProvider(ContentProvider targetProvider) {
        super();
        mTargetProvider = targetProvider;
    }

    public void attachInfo(Context context, ProviderInfo info) {
        // With newer Robolectric versions, we must create the target provider
        // before calling into super.  If we don't do this, the target
        // provider's onCreate() will witness a null getContext(), which the
        // Android documentation guarantees never happens on device.
        mTargetProvider.attachInfo(context, null);
        super.attachInfo(context, info);
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

    @Nullable
    @Override
    public Bundle call(String method, String arg, Bundle extras) {
        return mTargetProvider.call(method, arg, extras);
    }

    public void shutdown() {
        mTargetProvider.shutdown();
    }

    public ContentProvider getTargetProvider() {
        return mTargetProvider;
    }
}
