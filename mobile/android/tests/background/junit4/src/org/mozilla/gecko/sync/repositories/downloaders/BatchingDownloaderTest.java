/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.downloaders;

import android.net.Uri;
import android.os.SystemClock;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.CollectionConcurrentModificationException;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.InfoCollections;
import org.mozilla.gecko.sync.InfoConfiguration;
import org.mozilla.gecko.sync.SyncDeadlineReachedException;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.SyncResponse;
import org.mozilla.gecko.sync.net.SyncStorageCollectionRequest;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.repositories.NonPersistentRepositoryStateProvider;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.RepositoryStateProvider;
import org.mozilla.gecko.sync.repositories.Server15Repository;
import org.mozilla.gecko.sync.repositories.Server15RepositorySession;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.message.BasicHttpResponse;
import ch.boye.httpclientandroidlib.message.BasicStatusLine;

import static org.junit.Assert.*;
import static org.junit.Assert.assertEquals;

@RunWith(TestRunner.class)
public class BatchingDownloaderTest {
    private MockSever15Repository serverRepository;
    private Server15RepositorySession repositorySession;
    private MockSessionFetchRecordsDelegate sessionFetchRecordsDelegate;
    private MockDownloader mockDownloader;
    private String DEFAULT_COLLECTION_NAME = "dummyCollection";
    private static String DEFAULT_COLLECTION_URL = "http://dummy.url/";
    private long DEFAULT_NEWER = 1;
    private String DEFAULT_SORT = "oldest";
    private String DEFAULT_IDS = "1";
    private String DEFAULT_LMHEADER = "12345678";

    private CountingShadowRepositoryState repositoryStateProvider;

    // Memory-backed state implementation which keeps a shadow of current values, so that they can be
    // queried by the tests. Classes under test do not have access to the shadow, and follow regular
    // non-persistent state semantics (value changes visible only after commit).
    static class CountingShadowRepositoryState extends NonPersistentRepositoryStateProvider {
        private AtomicInteger commitCount = new AtomicInteger(0);
        private final Map<String, Object> shadowMap = Collections.synchronizedMap(new HashMap<String, Object>(2));

        @Override
        public boolean commit() {
            commitCount.incrementAndGet();
            return super.commit();
        }

        int getCommitCount() {
            return commitCount.get();
        }

        @Nullable
        public Long getShadowedLong(String key) {
            return (Long) shadowMap.get(key);
        }

        @Nullable
        public String getShadowedString(String key) {
            return (String) shadowMap.get(key);
        }

        @Override
        public CountingShadowRepositoryState setLong(String key, Long value) {
            shadowMap.put(key, value);
            super.setLong(key, value);
            return this;
        }

        @Override
        public CountingShadowRepositoryState setString(String key, String value) {
            shadowMap.put(key, value);
            super.setString(key, value);
            return this;
        }

        @Override
        public CountingShadowRepositoryState clear(String key) {
            shadowMap.remove(key);
            super.clear(key);
            return this;
        }

        @Override
        public boolean resetAndCommit() {
            shadowMap.clear();
            return super.resetAndCommit();
        }
    }

    static class MockSessionFetchRecordsDelegate implements RepositorySessionFetchRecordsDelegate {
        public boolean isFailure;
        public boolean isFetched;
        public boolean isSuccess;
        public int batchesCompleted;
        public Exception ex;
        public Record record;

        @Override
        public void onFetchFailed(Exception ex) {
            this.isFailure = true;
            this.ex = ex;
        }

        @Override
        public void onFetchedRecord(Record record) {
            this.isFetched = true;
            this.record = record;
        }

        @Override
        public void onFetchCompleted() {
            this.isSuccess = true;
        }

        @Override
        public void onBatchCompleted() {
            this.batchesCompleted += 1;
        }

        @Override
        public RepositorySessionFetchRecordsDelegate deferredFetchDelegate(ExecutorService executor) {
            return null;
        }
    }

    static class MockRequest extends SyncStorageCollectionRequest {

        MockRequest(URI uri) {
            super(uri);
        }

        @Override
        public void get() {

        }
    }

    static class MockDownloader extends BatchingDownloader {
        public long newer;
        public long limit;
        public boolean full;
        public String sort;
        public String ids;
        public String offset;
        public boolean abort;

        MockDownloader(RepositorySession repositorySession, boolean allowMultipleBatches, boolean keepTrackOfHighWaterMark, RepositoryStateProvider repositoryStateProvider) {
            super(null, Uri.EMPTY, SystemClock.elapsedRealtime() + TimeUnit.MINUTES.toMillis(30),
                    allowMultipleBatches, keepTrackOfHighWaterMark, repositoryStateProvider, repositorySession);
        }

        @Override
        public void fetchWithParameters(long newer,
                                 long batchLimit,
                                 boolean full,
                                 String sort,
                                 String ids,
                                 SyncStorageCollectionRequest request,
                                 RepositorySessionFetchRecordsDelegate fetchRecordsDelegate)
                throws UnsupportedEncodingException, URISyntaxException {
            this.newer = newer;
            this.limit = batchLimit;
            this.full = full;
            this.sort = sort;
            this.ids = ids;
            MockRequest mockRequest = new MockRequest(new URI(DEFAULT_COLLECTION_URL));
            super.fetchWithParameters(newer, batchLimit, full, sort, ids, mockRequest, fetchRecordsDelegate);
        }

        @Override
        public void abortRequests() {
            this.abort = true;
        }

        @Override
        public SyncStorageCollectionRequest makeSyncStorageCollectionRequest(long newer,
                      long batchLimit,
                      boolean full,
                      String sort,
                      String ids,
                      String offset)
                throws URISyntaxException, UnsupportedEncodingException {
            this.offset = offset;
            return super.makeSyncStorageCollectionRequest(newer, batchLimit, full, sort, ids, offset);
        }
    }

    static class MockSever15Repository extends Server15Repository {
        MockSever15Repository(@NonNull String collection, @NonNull String storageURL,
                                     AuthHeaderProvider authHeaderProvider, @NonNull InfoCollections infoCollections,
                                     @NonNull InfoConfiguration infoConfiguration) throws URISyntaxException {
            super(collection, SystemClock.elapsedRealtime() + TimeUnit.MINUTES.toMillis(30),
                    storageURL, authHeaderProvider, infoCollections, infoConfiguration,
                    new NonPersistentRepositoryStateProvider());
        }
    }

    static class MockRepositorySession extends Server15RepositorySession {
        public boolean abort;

        MockRepositorySession(Repository repository) {
            super(repository);
        }

        @Override
        public void abort() {
            this.abort = true;
        }
    }

    @Before
    public void setUp() throws Exception {
        sessionFetchRecordsDelegate = new MockSessionFetchRecordsDelegate();

        serverRepository = new MockSever15Repository(DEFAULT_COLLECTION_NAME, DEFAULT_COLLECTION_URL, null,
                new InfoCollections(), new InfoConfiguration());
        repositorySession = new Server15RepositorySession(serverRepository);
        repositoryStateProvider = new CountingShadowRepositoryState();
        mockDownloader = new MockDownloader(repositorySession, true, true, repositoryStateProvider);
    }

    @Test
    public void testFlattenId() {
        String[] emptyGuid = new String[]{};
        String flatten =  BatchingDownloader.flattenIDs(emptyGuid);
        assertEquals("", flatten);

        String guid0 = "123456789abc";
        String[] singleGuid = new String[1];
        singleGuid[0] = guid0;
        flatten = BatchingDownloader.flattenIDs(singleGuid);
        assertEquals("123456789abc", flatten);

        String guid1 = "456789abc";
        String guid2 = "789abc";
        String[] multiGuid = new String[3];
        multiGuid[0] = guid0;
        multiGuid[1] = guid1;
        multiGuid[2] = guid2;
        flatten = BatchingDownloader.flattenIDs(multiGuid);
        assertEquals("123456789abc,456789abc,789abc", flatten);
    }

    @Test
    public void testBatchingTrivial() throws Exception {
        MockDownloader mockDownloader = new MockDownloader(repositorySession, true, true, repositoryStateProvider);

        assertNull(mockDownloader.getLastModified());
        // Number of records == batch limit.
        final long BATCH_LIMIT = 100;
        mockDownloader.fetchSince(sessionFetchRecordsDelegate, DEFAULT_NEWER, BATCH_LIMIT, DEFAULT_SORT, null);

        SyncStorageResponse response = makeSyncStorageResponse(200, DEFAULT_LMHEADER, null, "100");
        SyncStorageCollectionRequest request = new SyncStorageCollectionRequest(new URI(DEFAULT_COLLECTION_URL));
        mockDownloader.onFetchCompleted(response, sessionFetchRecordsDelegate, request,
                DEFAULT_NEWER, BATCH_LIMIT, true, DEFAULT_SORT, DEFAULT_IDS);

        assertEquals(DEFAULT_LMHEADER, mockDownloader.getLastModified());
        assertTrue(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);
        assertEquals(0, sessionFetchRecordsDelegate.batchesCompleted);

        // NB: we set highWaterMark as part of onFetchedRecord, so we don't expect it to be set here.
        // Expect no offset to be persisted.
        ensureOffsetContextIsNull(repositoryStateProvider);
        assertEquals(1, repositoryStateProvider.getCommitCount());
    }

    @Test
    public void testBatchingSingleBatchMode() throws Exception {
        MockDownloader mockDownloader = new MockDownloader(repositorySession, false, true, repositoryStateProvider);

        assertNull(mockDownloader.getLastModified());
        // Number of records > batch limit. But, we're only allowed to make one batch request.
        final long BATCH_LIMIT = 100;
        mockDownloader.fetchSince(sessionFetchRecordsDelegate, DEFAULT_NEWER, BATCH_LIMIT, DEFAULT_SORT, null);

        String offsetHeader = "25";
        String recordsHeader = "500";
        SyncStorageResponse response = makeSyncStorageResponse(200, DEFAULT_LMHEADER, offsetHeader, recordsHeader);
        SyncStorageCollectionRequest request = new SyncStorageCollectionRequest(new URI(DEFAULT_COLLECTION_URL));
        mockDownloader.onFetchCompleted(response, sessionFetchRecordsDelegate, request,
                DEFAULT_NEWER, BATCH_LIMIT, true, DEFAULT_SORT, DEFAULT_IDS);

        assertEquals(DEFAULT_LMHEADER, mockDownloader.getLastModified());
        assertTrue(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);
        assertEquals(0, sessionFetchRecordsDelegate.batchesCompleted);

        // We don't care about the offset in a single batch mode.
        ensureOffsetContextIsNull(repositoryStateProvider);
        assertEquals(1, repositoryStateProvider.getCommitCount());
    }

    @Test
    public void testBatching() throws Exception {
        final long BATCH_LIMIT = 25;
        mockDownloader = new MockDownloader(repositorySession, true, true, repositoryStateProvider);

        assertNull(mockDownloader.getLastModified());
        mockDownloader.fetchSince(sessionFetchRecordsDelegate, DEFAULT_NEWER, BATCH_LIMIT, DEFAULT_SORT, null);

        final String recordsHeader = "25";
        performOnFetchCompleted("25", recordsHeader, BATCH_LIMIT);

        assertEquals(DEFAULT_LMHEADER, mockDownloader.getLastModified());
        // Verify the same parameters are used in the next fetch.
        assertSameParameters(mockDownloader, BATCH_LIMIT);
        assertEquals("25", mockDownloader.offset);
        assertFalse(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);
        assertEquals(1, sessionFetchRecordsDelegate.batchesCompleted);

        // Offset context set.
        ensureOffsetContextIs(repositoryStateProvider, "25", "oldest", 1L);
        assertEquals(1, repositoryStateProvider.getCommitCount());

        // The next batch, we still have an offset token and has not exceed the total limit.
        performOnFetchCompleted("50", recordsHeader, BATCH_LIMIT);

        assertEquals(DEFAULT_LMHEADER, mockDownloader.getLastModified());
        // Verify the same parameters are used in the next fetch.
        assertSameParameters(mockDownloader, BATCH_LIMIT);
        assertEquals("50", mockDownloader.offset);
        assertFalse(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);
        assertEquals(2, sessionFetchRecordsDelegate.batchesCompleted);

        // Offset context updated.
        ensureOffsetContextIs(repositoryStateProvider, "50", "oldest", 1L);
        assertEquals(2, repositoryStateProvider.getCommitCount());

        // The next batch, we still have an offset token and has not exceed the total limit.
        performOnFetchCompleted("75", recordsHeader, BATCH_LIMIT);

        assertEquals(DEFAULT_LMHEADER, mockDownloader.getLastModified());
        // Verify the same parameters are used in the next fetch.
        assertSameParameters(mockDownloader, BATCH_LIMIT);
        assertEquals("75", mockDownloader.offset);
        assertFalse(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);
        assertEquals(3, sessionFetchRecordsDelegate.batchesCompleted);

        // Offset context updated.
        ensureOffsetContextIs(repositoryStateProvider, "75", "oldest", 1L);
        assertEquals(3, repositoryStateProvider.getCommitCount());

        // No more offset token, so we complete batching.
        performOnFetchCompleted(null, recordsHeader, BATCH_LIMIT);

        assertEquals(DEFAULT_LMHEADER, mockDownloader.getLastModified());
        assertTrue(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);
        assertEquals(3, sessionFetchRecordsDelegate.batchesCompleted);

        // Offset context cleared since we finished batching, and committed.
        ensureOffsetContextIsNull(repositoryStateProvider);
        assertEquals(4, repositoryStateProvider.getCommitCount());
    }

    @Test
    public void testFailureLMChangedMultiBatch() throws Exception {
        assertNull(mockDownloader.getLastModified());

        String lmHeader = "12345678";
        String offsetHeader = "100";
        SyncStorageResponse response = makeSyncStorageResponse(200, lmHeader, offsetHeader, null);
        SyncStorageCollectionRequest request = new SyncStorageCollectionRequest(new URI("http://dummy.url"));
        long limit = 1;
        mockDownloader.onFetchCompleted(response, sessionFetchRecordsDelegate, request, DEFAULT_NEWER,
                limit, true, DEFAULT_SORT, DEFAULT_IDS);

        assertEquals(lmHeader, mockDownloader.getLastModified());
        // Verify the same parameters are used in the next fetch.
        assertEquals(DEFAULT_NEWER, mockDownloader.newer);
        assertEquals(limit, mockDownloader.limit);
        assertTrue(mockDownloader.full);
        assertEquals(DEFAULT_SORT, mockDownloader.sort);
        assertEquals(DEFAULT_IDS, mockDownloader.ids);
        assertEquals(offsetHeader, mockDownloader.offset);
        assertFalse(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);

        // Offset context set.
        ensureOffsetContextIs(repositoryStateProvider, "100", "oldest", 1L);
        assertEquals(1, repositoryStateProvider.getCommitCount());

        // Last modified header somehow changed.
        lmHeader = "10000000";
        response = makeSyncStorageResponse(200, lmHeader, offsetHeader, null);
        mockDownloader.onFetchCompleted(response, sessionFetchRecordsDelegate, request, DEFAULT_NEWER,
                limit, true, DEFAULT_SORT, DEFAULT_IDS);

        assertNotEquals(lmHeader, mockDownloader.getLastModified());
        assertTrue(mockDownloader.abort);
        assertFalse(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertTrue(sessionFetchRecordsDelegate.isFailure);

        // Since we failed due to a remotely modified collection, we expect offset context to be reset.
        ensureOffsetContextIsNull(repositoryStateProvider);
        assertEquals(2, repositoryStateProvider.getCommitCount());
    }

    @Test
    public void testFailureException() throws Exception {
        Exception ex = new IllegalStateException();
        SyncStorageCollectionRequest request = new SyncStorageCollectionRequest(new URI("http://dummy.url"));
        mockDownloader.handleFetchFailed(sessionFetchRecordsDelegate, ex, request);

        assertFalse(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertTrue(sessionFetchRecordsDelegate.isFailure);
        assertEquals(ex.getClass(), sessionFetchRecordsDelegate.ex.getClass());
        assertNull(sessionFetchRecordsDelegate.record);
    }

    @Test
    public void testOffsetNotResetAfterDeadline() throws Exception {
        final long BATCH_LIMIT = 25;
        mockDownloader = new MockDownloader(repositorySession, true, true, repositoryStateProvider);
        mockDownloader.fetchSince(sessionFetchRecordsDelegate, DEFAULT_NEWER, BATCH_LIMIT, DEFAULT_SORT, null);

        SyncStorageCollectionRequest request = performOnFetchCompleted("25", "25", 25);

        // Offset context set.
        ensureOffsetContextIs(repositoryStateProvider, "25", "oldest", 1L);
        assertEquals(1, repositoryStateProvider.getCommitCount());

        Exception ex = new SyncDeadlineReachedException();
        mockDownloader.handleFetchFailed(sessionFetchRecordsDelegate, ex, request);

        // Offset context not reset.
        ensureOffsetContextIs(repositoryStateProvider, "25", "oldest", 1L);
        assertEquals(2, repositoryStateProvider.getCommitCount());
    }

    @Test
    public void testOffsetResetAfterConcurrentModification() throws Exception {
        final long BATCH_LIMIT = 25;
        mockDownloader = new MockDownloader(repositorySession, true, true, repositoryStateProvider);
        mockDownloader.fetchSince(sessionFetchRecordsDelegate, DEFAULT_NEWER, BATCH_LIMIT, DEFAULT_SORT, null);

        SyncStorageCollectionRequest request = performOnFetchCompleted("25", "25", 25);

        // Offset context set.
        ensureOffsetContextIs(repositoryStateProvider, "25", "oldest", 1L);
        assertEquals(1, repositoryStateProvider.getCommitCount());

        Exception ex = new CollectionConcurrentModificationException();
        mockDownloader.handleFetchFailed(sessionFetchRecordsDelegate, ex, request);

        // Offset is reset.
        ensureOffsetContextIsNull(repositoryStateProvider);
        assertEquals(2, repositoryStateProvider.getCommitCount());
    }

    @Test
    public void testFetchRecord() {
        CryptoRecord record = new CryptoRecord();
        mockDownloader.onFetchedRecord(record, sessionFetchRecordsDelegate);

        assertTrue(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFailure);
        assertEquals(record, sessionFetchRecordsDelegate.record);
    }

    @Test
    public void testHighWaterMarkTracking() {
        CryptoRecord record = new CryptoRecord();

        // HWM enabled
        mockDownloader = new MockDownloader(repositorySession, true, true, repositoryStateProvider);

        record.lastModified = 1L;
        mockDownloader.onFetchedRecord(record, sessionFetchRecordsDelegate);
        assertEquals(Long.valueOf(1), repositoryStateProvider.getShadowedLong(RepositoryStateProvider.KEY_HIGH_WATER_MARK));

        record.lastModified = 5L;
        mockDownloader.onFetchedRecord(record, sessionFetchRecordsDelegate);
        assertEquals(Long.valueOf(5), repositoryStateProvider.getShadowedLong(RepositoryStateProvider.KEY_HIGH_WATER_MARK));

        // NB: Currently nothing is preventing HWM from "going down".
        record.lastModified = 4L;
        mockDownloader.onFetchedRecord(record, sessionFetchRecordsDelegate);
        assertEquals(Long.valueOf(4), repositoryStateProvider.getShadowedLong(RepositoryStateProvider.KEY_HIGH_WATER_MARK));

        // HWM disabled
        mockDownloader = new MockDownloader(repositorySession, true, false, repositoryStateProvider);
        assertTrue(repositoryStateProvider.resetAndCommit());

        record.lastModified = 4L;
        mockDownloader.onFetchedRecord(record, sessionFetchRecordsDelegate);
        assertNull(repositoryStateProvider.getShadowedLong(RepositoryStateProvider.KEY_HIGH_WATER_MARK));

        record.lastModified = 5L;
        mockDownloader.onFetchedRecord(record, sessionFetchRecordsDelegate);
        assertNull(repositoryStateProvider.getShadowedLong(RepositoryStateProvider.KEY_HIGH_WATER_MARK));
    }

    @Test
    public void testAbortRequests() {
        MockRepositorySession mockRepositorySession = new MockRepositorySession(serverRepository);
        BatchingDownloader downloader = new BatchingDownloader(
                null, Uri.EMPTY, SystemClock.elapsedRealtime() + TimeUnit.MINUTES.toMillis(30),
                true, true, new NonPersistentRepositoryStateProvider(), mockRepositorySession);
        assertFalse(mockRepositorySession.abort);
        downloader.abortRequests();
        assertTrue(mockRepositorySession.abort);
    }

    @Test
    public void testBuildCollectionURI() {
        try {
            assertEquals("?full=1&newer=5000.000", BatchingDownloader.buildCollectionURI(Uri.EMPTY, true, 5000000L, -1, null, null, null).toString());
            assertEquals("?newer=1230.000", BatchingDownloader.buildCollectionURI(Uri.EMPTY, false, 1230000L, -1, null, null, null).toString());
            assertEquals("?newer=5000.000&limit=10", BatchingDownloader.buildCollectionURI(Uri.EMPTY, false, 5000000L, 10, null, null, null).toString());
            assertEquals("?full=1&newer=5000.000&sort=index", BatchingDownloader.buildCollectionURI(Uri.EMPTY, true, 5000000L, 0, "index", null, null).toString());
            assertEquals("?full=1&ids=123%2Cabc", BatchingDownloader.buildCollectionURI(Uri.EMPTY, true, -1L, -1, null, "123,abc", null).toString());

            final Uri baseUri = Uri.parse("https://moztest.org/collection/");
            assertEquals(baseUri + "?full=1&ids=123%2Cabc&offset=1234", BatchingDownloader.buildCollectionURI(baseUri, true, -1L, -1, null, "123,abc", "1234").toString());
        } catch (URISyntaxException e) {
            fail();
        }
    }

    private SyncStorageCollectionRequest performOnFetchCompleted(String offsetHeader, String recordsHeader, long batchLimit) throws URISyntaxException {
        SyncStorageResponse response = makeSyncStorageResponse(200,  DEFAULT_LMHEADER, offsetHeader, recordsHeader);
        SyncStorageCollectionRequest request = new SyncStorageCollectionRequest(new URI(DEFAULT_COLLECTION_URL));
        mockDownloader.onFetchCompleted(response, sessionFetchRecordsDelegate, request, DEFAULT_NEWER,
                batchLimit, true, DEFAULT_SORT, DEFAULT_IDS);

        return request;
    }

    private void assertSameParameters(MockDownloader mockDownloader, long limit) {
        assertEquals(DEFAULT_NEWER, mockDownloader.newer);
        assertEquals(limit, mockDownloader.limit);
        assertTrue(mockDownloader.full);
        assertEquals(DEFAULT_SORT, mockDownloader.sort);
        assertEquals(DEFAULT_IDS, mockDownloader.ids);
    }

    private SyncStorageResponse makeSyncStorageResponse(int code, String lastModified, String offset, String records) {
        BasicHttpResponse response = new BasicHttpResponse(
                new BasicStatusLine(new ProtocolVersion("HTTP", 1, 1), code, null));

        if (lastModified != null) {
            response.addHeader(SyncResponse.X_LAST_MODIFIED, lastModified);
        }

        if (offset != null) {
            response.addHeader(SyncResponse.X_WEAVE_NEXT_OFFSET, offset);
        }

        if (records != null) {
            response.addHeader(SyncResponse.X_WEAVE_RECORDS, records);
        }

        return new SyncStorageResponse(response);
    }

    private void ensureOffsetContextIsNull(CountingShadowRepositoryState stateProvider) {
        assertNull(stateProvider.getShadowedString(RepositoryStateProvider.KEY_OFFSET));
        assertNull(stateProvider.getShadowedString(RepositoryStateProvider.KEY_OFFSET_ORDER));
        assertNull(stateProvider.getShadowedLong(RepositoryStateProvider.KEY_OFFSET_SINCE));
    }

    private void ensureOffsetContextIs(CountingShadowRepositoryState stateProvider, String offset, String order, Long since) {
        assertEquals(offset, stateProvider.getShadowedString(RepositoryStateProvider.KEY_OFFSET));
        assertEquals(since, stateProvider.getShadowedLong(RepositoryStateProvider.KEY_OFFSET_SINCE));
        assertEquals(order, stateProvider.getShadowedString(RepositoryStateProvider.KEY_OFFSET_ORDER));
    }
}
