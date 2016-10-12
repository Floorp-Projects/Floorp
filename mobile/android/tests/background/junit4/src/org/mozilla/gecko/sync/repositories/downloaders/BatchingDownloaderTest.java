/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.downloaders;

import android.net.Uri;
import android.os.SystemClock;
import android.support.annotation.NonNull;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.InfoCollections;
import org.mozilla.gecko.sync.InfoConfiguration;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.SyncResponse;
import org.mozilla.gecko.sync.net.SyncStorageCollectionRequest;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.Server11Repository;
import org.mozilla.gecko.sync.repositories.Server11RepositorySession;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.TimeUnit;

import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.message.BasicHttpResponse;
import ch.boye.httpclientandroidlib.message.BasicStatusLine;

import static org.junit.Assert.*;
import static org.junit.Assert.assertEquals;

@RunWith(TestRunner.class)
public class BatchingDownloaderTest {
    private MockSever11Repository serverRepository;
    private Server11RepositorySession repositorySession;
    private MockSessionFetchRecordsDelegate sessionFetchRecordsDelegate;
    private MockDownloader mockDownloader;
    private String DEFAULT_COLLECTION_NAME = "dummyCollection";
    private String DEFAULT_COLLECTION_URL = "http://dummy.url/";
    private long DEFAULT_NEWER = 1;
    private String DEFAULT_SORT = "oldest";
    private String DEFAULT_IDS = "1";
    private String DEFAULT_LMHEADER = "12345678";

    class MockSessionFetchRecordsDelegate implements RepositorySessionFetchRecordsDelegate {
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
        public void onFetchCompleted(long fetchEnd) {
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

    class MockRequest extends SyncStorageCollectionRequest {

        public MockRequest(URI uri) {
            super(uri);
        }

        @Override
        public void get() {

        }
    }

    class MockDownloader extends BatchingDownloader {
        public long newer;
        public long limit;
        public boolean full;
        public String sort;
        public String ids;
        public String offset;
        public boolean abort;

        public MockDownloader(RepositorySession repositorySession, boolean allowMultipleBatches) {
            super(null, Uri.EMPTY, SystemClock.elapsedRealtime() + TimeUnit.MINUTES.toMillis(30), allowMultipleBatches, repositorySession);
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

    class MockSever11Repository extends Server11Repository {
        public MockSever11Repository(@NonNull String collection, @NonNull String storageURL,
                                     AuthHeaderProvider authHeaderProvider, @NonNull InfoCollections infoCollections,
                                     @NonNull InfoConfiguration infoConfiguration) throws URISyntaxException {
            super(collection, SystemClock.elapsedRealtime() + TimeUnit.MINUTES.toMillis(30), storageURL, authHeaderProvider, infoCollections, infoConfiguration);
        }
    }

    class MockRepositorySession extends Server11RepositorySession {
        public boolean abort;

        public MockRepositorySession(Repository repository) {
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

        serverRepository = new MockSever11Repository(DEFAULT_COLLECTION_NAME, DEFAULT_COLLECTION_URL, null,
                new InfoCollections(), new InfoConfiguration());
        repositorySession = new Server11RepositorySession(serverRepository);
        mockDownloader = new MockDownloader(repositorySession, true);
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
        MockDownloader mockDownloader = new MockDownloader(repositorySession, true);

        assertNull(mockDownloader.getLastModified());
        // Number of records == batch limit.
        final long BATCH_LIMIT = 100;
        mockDownloader.fetchSince(sessionFetchRecordsDelegate, DEFAULT_NEWER, BATCH_LIMIT, DEFAULT_SORT);

        SyncStorageResponse response = makeSyncStorageResponse(200, DEFAULT_LMHEADER, null, "100");
        SyncStorageCollectionRequest request = new SyncStorageCollectionRequest(new URI(DEFAULT_COLLECTION_URL));
        mockDownloader.onFetchCompleted(response, sessionFetchRecordsDelegate, request,
                DEFAULT_NEWER, BATCH_LIMIT, true, DEFAULT_SORT, DEFAULT_IDS);

        assertEquals(DEFAULT_LMHEADER, mockDownloader.getLastModified());
        assertTrue(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);
        assertEquals(0, sessionFetchRecordsDelegate.batchesCompleted);
    }

    @Test
    public void testBatchingSingleBatchMode() throws Exception {
        MockDownloader mockDownloader = new MockDownloader(repositorySession, false);

        assertNull(mockDownloader.getLastModified());
        // Number of records > batch limit. But, we're only allowed to make one batch request.
        final long BATCH_LIMIT = 100;
        mockDownloader.fetchSince(sessionFetchRecordsDelegate, DEFAULT_NEWER, BATCH_LIMIT, DEFAULT_SORT);

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
    }

    @Test
    public void testBatching() throws Exception {
        final long BATCH_LIMIT = 25;
        mockDownloader = new MockDownloader(repositorySession, true);

        assertNull(mockDownloader.getLastModified());
        mockDownloader.fetchSince(sessionFetchRecordsDelegate, DEFAULT_NEWER, BATCH_LIMIT, DEFAULT_SORT);

        String offsetHeader = "25";
        String recordsHeader = "25";
        SyncStorageResponse response = makeSyncStorageResponse(200,  DEFAULT_LMHEADER, offsetHeader, recordsHeader);
        SyncStorageCollectionRequest request = new SyncStorageCollectionRequest(new URI(DEFAULT_COLLECTION_URL));
        mockDownloader.onFetchCompleted(response, sessionFetchRecordsDelegate, request, DEFAULT_NEWER,
                BATCH_LIMIT, true, DEFAULT_SORT, DEFAULT_IDS);

        assertEquals(DEFAULT_LMHEADER, mockDownloader.getLastModified());
        // Verify the same parameters are used in the next fetch.
        assertSameParameters(mockDownloader, BATCH_LIMIT);
        assertEquals(offsetHeader, mockDownloader.offset);
        assertFalse(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);
        assertEquals(1, sessionFetchRecordsDelegate.batchesCompleted);

        // The next batch, we still have an offset token and has not exceed the total limit.
        offsetHeader = "50";
        response = makeSyncStorageResponse(200, DEFAULT_LMHEADER, offsetHeader, recordsHeader);
        mockDownloader.onFetchCompleted(response, sessionFetchRecordsDelegate, request, DEFAULT_NEWER,
                BATCH_LIMIT, true, DEFAULT_SORT, DEFAULT_IDS);

        assertEquals(DEFAULT_LMHEADER, mockDownloader.getLastModified());
        // Verify the same parameters are used in the next fetch.
        assertSameParameters(mockDownloader, BATCH_LIMIT);
        assertEquals(offsetHeader, mockDownloader.offset);
        assertFalse(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);
        assertEquals(2, sessionFetchRecordsDelegate.batchesCompleted);

        // The next batch, we still have an offset token and has not exceed the total limit.
        offsetHeader = "75";
        response = makeSyncStorageResponse(200, DEFAULT_LMHEADER, offsetHeader, recordsHeader);
        mockDownloader.onFetchCompleted(response, sessionFetchRecordsDelegate, request, DEFAULT_NEWER,
                BATCH_LIMIT, true, DEFAULT_SORT, DEFAULT_IDS);

        assertEquals(DEFAULT_LMHEADER, mockDownloader.getLastModified());
        // Verify the same parameters are used in the next fetch.
        assertSameParameters(mockDownloader, BATCH_LIMIT);
        assertEquals(offsetHeader, mockDownloader.offset);
        assertFalse(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);
        assertEquals(3, sessionFetchRecordsDelegate.batchesCompleted);

        // No more offset token, so we complete batching.
        response = makeSyncStorageResponse(200, DEFAULT_LMHEADER, null, recordsHeader);
        mockDownloader.onFetchCompleted(response, sessionFetchRecordsDelegate, request, DEFAULT_NEWER,
                BATCH_LIMIT, true, DEFAULT_SORT, DEFAULT_IDS);

        assertEquals(DEFAULT_LMHEADER, mockDownloader.getLastModified());
        assertTrue(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);
        assertEquals(3, sessionFetchRecordsDelegate.batchesCompleted);
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
    }

    @Test
    public void testFailureException() throws Exception {
        Exception ex = new IllegalStateException();
        SyncStorageCollectionRequest request = new SyncStorageCollectionRequest(new URI("http://dummy.url"));
        mockDownloader.onFetchFailed(ex, sessionFetchRecordsDelegate, request);

        assertFalse(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertTrue(sessionFetchRecordsDelegate.isFailure);
        assertEquals(ex.getClass(), sessionFetchRecordsDelegate.ex.getClass());
        assertNull(sessionFetchRecordsDelegate.record);
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
    public void testAbortRequests() {
        MockRepositorySession mockRepositorySession = new MockRepositorySession(serverRepository);
        BatchingDownloader downloader = new BatchingDownloader(null, Uri.EMPTY, SystemClock.elapsedRealtime() + TimeUnit.MINUTES.toMillis(30), true, mockRepositorySession);
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
}
