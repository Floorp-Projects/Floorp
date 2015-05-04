/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import java.io.File;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.concurrent.Callable;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserProvider;

import android.content.ContentProvider;
import android.content.ContentProviderOperation;
import android.content.ContentProviderResult;
import android.content.ContentValues;
import android.content.Context;
import android.content.OperationApplicationException;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.database.ContentObserver;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.test.IsolatedContext;
import android.test.RenamingDelegatingContext;
import android.test.mock.MockContentResolver;
import android.test.mock.MockContext;

/*
 * ContentProviderTest provides the infrastructure to run content provider
 * tests in an controlled/isolated environment, guaranteeing that your tests
 * will not affect or be affected by any UI-related code. This is basically
 * a heavily adapted port of Android's ProviderTestCase2 to work on Mozilla's
 * infrastructure.
 *
 * For some tests, we need to have access to UI parts, or at least launch
 * the activity so the assets with test data become available, which requires
 * that we derive this test from BaseTest and consequently pull in some more
 * UI code than we'd ideally want. Furthermore, we need to pass the
 * Activity and not the instrumentation Context for the UI part to find some
 * of its required resources.
 * Similarly, we need to pass the Activity instead of the Instrumentation
 * Context down to some of our users (still wrapped in the Delegating Provider)
 * because they will stop working correctly if we do not. A typical problem
 * is that databases used in the ContentProvider will be attempted to be
 * opened twice.
 */
abstract class ContentProviderTest extends BaseTest {
    protected ContentProvider mProvider;
    protected ChangeRecordingMockContentResolver mResolver;
    protected ArrayList<Runnable> mTests;
    protected String mDatabaseName;
    protected String mProviderAuthority;
    protected IsolatedContext mProviderContext;

    private class ContentProviderMockContext extends MockContext {
        @Override
        public Resources getResources() {
            // We will fail to find some resources if we don't point
            // at the original activity.
            return ((Context)getActivity()).getResources();
        }

        @Override
        public String getPackageName() {
            return getInstrumentation().getContext().getPackageName();
        }

        @Override
        public String getPackageResourcePath() {
            return getInstrumentation().getContext().getPackageResourcePath();
        }

        @Override
        public File getDir(String name, int mode) {
            return getInstrumentation().getContext().getDir(this.getClass().getSimpleName() + "_" + name, mode);
        }

        @Override
        public Context getApplicationContext() {
            return this;
        }

        @Override
        public SharedPreferences getSharedPreferences(String name, int mode) {
            return getInstrumentation().getContext().getSharedPreferences(name, mode);
        }

        @Override
        public ApplicationInfo getApplicationInfo() {
            return getInstrumentation().getContext().getApplicationInfo();
        }
    }

    protected class DelegatingTestContentProvider extends ContentProvider {
        ContentProvider mTargetProvider;

        public DelegatingTestContentProvider(ContentProvider targetProvider) {
            super();
            mTargetProvider = targetProvider;
        }

        private Uri appendTestParam(Uri uri) {
            try {
                return appendUriParam(uri, BrowserContract.PARAM_IS_TEST, "1");
            } catch (Exception e) {}

            return null;
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
        public ContentProviderResult[] applyBatch (ArrayList<ContentProviderOperation> operations)
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

    /*
     * A MockContentResolver that records each URI that is supplied to
     * notifyChange.  Warning: the list of changed URIs is not
     * synchronized.
     */
    protected class ChangeRecordingMockContentResolver extends MockContentResolver {
        public final LinkedList<Uri> notifyChangeList = new LinkedList<Uri>();

        @Override
        public void notifyChange(Uri uri, ContentObserver observer, boolean syncToNetwork) {
            notifyChangeList.addLast(uri);

            super.notifyChange(uri, observer, syncToNetwork);
        }
    }

    /**
     * Factory function that makes new ContentProvider instances.
     * <p>
     * We want a fresh provider each test, so this should be invoked in
     * <code>setUp</code> before each individual test.
     */
    protected static Callable<ContentProvider> sBrowserProviderCallable = new Callable<ContentProvider>() {
        @Override
        public ContentProvider call() {
            return new BrowserProvider();
        }
    };

    private void setUpContentProvider(ContentProvider targetProvider) throws Exception {
        mResolver = new ChangeRecordingMockContentResolver();

        final String filenamePrefix = this.getClass().getSimpleName() + ".";
        RenamingDelegatingContext targetContextWrapper =
                new RenamingDelegatingContext(
                    new ContentProviderMockContext(),
                    (Context)getActivity(),
                    filenamePrefix);

        mProviderContext = new IsolatedContext(mResolver, targetContextWrapper);

        targetProvider.attachInfo(mProviderContext, null);

        mProvider = new DelegatingTestContentProvider(targetProvider);
        mProvider.attachInfo(mProviderContext, null);

        mResolver.addProvider(mProviderAuthority, mProvider);
    }

    public static Uri appendUriParam(Uri uri, String param, String value) {
        return uri.buildUpon().appendQueryParameter(param, value).build();
    }

    public void setTestName(String testName) {
        mAsserter.setTestName(this.getClass().getName() + " - " + testName);
    }

    @Override
    public void setUp() throws Exception {
        throw new UnsupportedOperationException("You should call setUp(authority, databaseName) instead");
    }

    public void setUp(Callable<ContentProvider> contentProviderFactory, String authority, String databaseName) throws Exception {
        super.setUp();

        mTests = new ArrayList<Runnable>();
        mDatabaseName = databaseName;

        mProviderAuthority = authority;

        setUpContentProvider(contentProviderFactory.call());
    }

    @Override
    public void tearDown() throws Exception {
        if (Build.VERSION.SDK_INT >= 11) {
            mProvider.shutdown();
        }

        if (mDatabaseName != null) {
            mProviderContext.deleteDatabase(mDatabaseName);
        }

        super.tearDown();
    }

    public AssetManager getAssetManager()  {
        return getInstrumentation().getContext().getAssets();
    }
}
