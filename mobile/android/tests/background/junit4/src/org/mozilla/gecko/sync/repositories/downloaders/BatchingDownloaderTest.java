/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.downloaders;

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
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.Server11Repository;
import org.mozilla.gecko.sync.repositories.Server11RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.concurrent.ExecutorService;

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
    private String DEFAULT_SORT = "index";
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
        public void onFetchFailed(Exception ex, Record record) {
            this.isFailure = true;
            this.ex = ex;
            this.record = record;
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

        public MockDownloader(Server11Repository repository, Server11RepositorySession repositorySession) {
            super(repository, repositorySession);
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
            super(collection, storageURL, authHeaderProvider, infoCollections, infoConfiguration);
        }

        @Override
        public long getDefaultTotalLimit() {
            return 200;
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
        mockDownloader = new MockDownloader(serverRepository, repositorySession);
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
    public void testEncodeParam() throws Exception {
        String param = "123&123";
        String encodedParam = mockDownloader.encodeParam(param);
        assertEquals("123%26123", encodedParam);
    }

    @Test(expected=IllegalArgumentException.class)
    public void testOverTotalLimit() throws Exception {
        // Per-batch limits exceed total.
        Server11Repository repository = new Server11Repository(DEFAULT_COLLECTION_NAME, DEFAULT_COLLECTION_URL,
                null, new InfoCollections(), new InfoConfiguration()) {
            @Override
            public long getDefaultTotalLimit() {
                return 100;
            }
            @Override
            public long getDefaultBatchLimit() {
                return 200;
            }
        };
        MockDownloader mockDownloader = new MockDownloader(repository, repositorySession);

        assertNull(mockDownloader.getLastModified());
        mockDownloader.fetchSince(DEFAULT_NEWER, sessionFetchRecordsDelegate);
    }

    @Test
    public void testTotalLimit() throws Exception {
        // Total and per-batch limits are the same.
        Server11Repository repository = new Server11Repository(DEFAULT_COLLECTION_NAME, DEFAULT_COLLECTION_URL,
                null, new InfoCollections(), new InfoConfiguration()) {
            @Override
            public long getDefaultTotalLimit() {
                return 100;
            }
            @Override
            public long getDefaultBatchLimit() {
                return 100;
            }
        };
        MockDownloader mockDownloader = new MockDownloader(repository, repositorySession);

        assertNull(mockDownloader.getLastModified());
        mockDownloader.fetchSince(DEFAULT_NEWER, sessionFetchRecordsDelegate);

        SyncStorageResponse response = makeSyncStorageResponse(200, DEFAULT_LMHEADER, "100", "100");
        SyncStorageCollectionRequest request = new SyncStorageCollectionRequest(new URI(DEFAULT_COLLECTION_URL));
        long limit = repository.getDefaultBatchLimit();
        mockDownloader.onFetchCompleted(response, sessionFetchRecordsDelegate, request,
                DEFAULT_NEWER, limit, true, DEFAULT_SORT, DEFAULT_IDS);

        assertEquals(DEFAULT_LMHEADER, mockDownloader.getLastModified());
        assertTrue(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);
    }

    @Test
    public void testOverHalfOfTotalLimit() throws Exception {
        // Per-batch limit is just a bit lower than total.
        Server11Repository repository = new Server11Repository(DEFAULT_COLLECTION_NAME, DEFAULT_COLLECTION_URL,
                null, new InfoCollections(), new InfoConfiguration()) {
            @Override
            public long getDefaultTotalLimit() {
                return 100;
            }
            @Override
            public long getDefaultBatchLimit() {
                return 75;
            }
        };
        MockDownloader mockDownloader = new MockDownloader(repository, repositorySession);

        assertNull(mockDownloader.getLastModified());
        mockDownloader.fetchSince(DEFAULT_NEWER, sessionFetchRecordsDelegate);

        String offsetHeader = "75";
        String recordsHeader = "75";
        SyncStorageResponse response = makeSyncStorageResponse(200,  DEFAULT_LMHEADER, offsetHeader, recordsHeader);
        SyncStorageCollectionRequest request = new SyncStorageCollectionRequest(new URI(DEFAULT_COLLECTION_URL));
        long limit = repository.getDefaultBatchLimit();

        mockDownloader.onFetchCompleted(response, sessionFetchRecordsDelegate, request, DEFAULT_NEWER,
                limit, true, DEFAULT_SORT, DEFAULT_IDS);

        assertEquals(DEFAULT_LMHEADER, mockDownloader.getLastModified());
        // Verify the same parameters are used in the next fetch.
        assertSameParameters(mockDownloader, limit);
        assertEquals(offsetHeader, mockDownloader.offset);
        assertFalse(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);

        // The next batch, we still have an offset token but we complete our fetch since we have reached the total limit.
        offsetHeader = "150";
        response = makeSyncStorageResponse(200, DEFAULT_LMHEADER, offsetHeader, recordsHeader);
        mockDownloader.onFetchCompleted(response, sessionFetchRecordsDelegate, request, DEFAULT_NEWER,
                limit, true, DEFAULT_SORT, DEFAULT_IDS);

        assertEquals(DEFAULT_LMHEADER, mockDownloader.getLastModified());
        assertTrue(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);
    }

    @Test
    public void testHalfOfTotalLimit() throws Exception {
        // Per-batch limit is half of total.
        Server11Repository repository = new Server11Repository(DEFAULT_COLLECTION_NAME, DEFAULT_COLLECTION_URL,
                null, new InfoCollections(), new InfoConfiguration()) {
            @Override
            public long getDefaultTotalLimit() {
                return 100;
            }
            @Override
            public long getDefaultBatchLimit() {
                return 50;
            }
        };
        mockDownloader = new MockDownloader(repository, repositorySession);

        assertNull(mockDownloader.getLastModified());
        mockDownloader.fetchSince(DEFAULT_NEWER, sessionFetchRecordsDelegate);

        String offsetHeader = "50";
        String recordsHeader = "50";
        SyncStorageResponse response = makeSyncStorageResponse(200,  DEFAULT_LMHEADER, offsetHeader, recordsHeader);
        SyncStorageCollectionRequest request = new SyncStorageCollectionRequest(new URI(DEFAULT_COLLECTION_URL));
        long limit = repository.getDefaultBatchLimit();
        mockDownloader.onFetchCompleted(response, sessionFetchRecordsDelegate, request, DEFAULT_NEWER,
                limit, true, DEFAULT_SORT, DEFAULT_IDS);

        assertEquals(DEFAULT_LMHEADER, mockDownloader.getLastModified());
        // Verify the same parameters are used in the next fetch.
        assertSameParameters(mockDownloader, limit);
        assertEquals(offsetHeader, mockDownloader.offset);
        assertFalse(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);

        // The next batch, we still have an offset token but we complete our fetch since we have reached the total limit.
        offsetHeader = "100";
        response = makeSyncStorageResponse(200, DEFAULT_LMHEADER, offsetHeader, recordsHeader);
        mockDownloader.onFetchCompleted(response, sessionFetchRecordsDelegate, request, DEFAULT_NEWER,
                limit, true, DEFAULT_SORT, DEFAULT_IDS);

        assertEquals(DEFAULT_LMHEADER, mockDownloader.getLastModified());
        assertTrue(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);
    }

    @Test
    public void testFractionOfTotalLimit() throws Exception {
        // Per-batch limit is a small fraction of the total.
        Server11Repository repository = new Server11Repository(DEFAULT_COLLECTION_NAME, DEFAULT_COLLECTION_URL,
                null, new InfoCollections(), new InfoConfiguration()) {
            @Override
            public long getDefaultTotalLimit() {
                return 100;
            }
            @Override
            public long getDefaultBatchLimit() {
                return 25;
            }
        };
        mockDownloader = new MockDownloader(repository, repositorySession);

        assertNull(mockDownloader.getLastModified());
        mockDownloader.fetchSince(DEFAULT_NEWER, sessionFetchRecordsDelegate);

        String offsetHeader = "25";
        String recordsHeader = "25";
        SyncStorageResponse response = makeSyncStorageResponse(200,  DEFAULT_LMHEADER, offsetHeader, recordsHeader);
        SyncStorageCollectionRequest request = new SyncStorageCollectionRequest(new URI(DEFAULT_COLLECTION_URL));
        long limit = repository.getDefaultBatchLimit();
        mockDownloader.onFetchCompleted(response, sessionFetchRecordsDelegate, request, DEFAULT_NEWER,
                limit, true, DEFAULT_SORT, DEFAULT_IDS);

        assertEquals(DEFAULT_LMHEADER, mockDownloader.getLastModified());
        // Verify the same parameters are used in the next fetch.
        assertSameParameters(mockDownloader, limit);
        assertEquals(offsetHeader, mockDownloader.offset);
        assertFalse(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);

        // The next batch, we still have an offset token and has not exceed the total limit.
        offsetHeader = "50";
        response = makeSyncStorageResponse(200, DEFAULT_LMHEADER, offsetHeader, recordsHeader);
        mockDownloader.onFetchCompleted(response, sessionFetchRecordsDelegate, request, DEFAULT_NEWER,
                limit, true, DEFAULT_SORT, DEFAULT_IDS);

        assertEquals(DEFAULT_LMHEADER, mockDownloader.getLastModified());
        // Verify the same parameters are used in the next fetch.
        assertSameParameters(mockDownloader, limit);
        assertEquals(offsetHeader, mockDownloader.offset);
        assertFalse(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);

        // The next batch, we still have an offset token and has not exceed the total limit.
        offsetHeader = "75";
        response = makeSyncStorageResponse(200, DEFAULT_LMHEADER, offsetHeader, recordsHeader);
        mockDownloader.onFetchCompleted(response, sessionFetchRecordsDelegate, request, DEFAULT_NEWER,
                limit, true, DEFAULT_SORT, DEFAULT_IDS);

        assertEquals(DEFAULT_LMHEADER, mockDownloader.getLastModified());
        // Verify the same parameters are used in the next fetch.
        assertSameParameters(mockDownloader, limit);
        assertEquals(offsetHeader, mockDownloader.offset);
        assertFalse(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);

        // The next batch, we still have an offset token but we complete our fetch since we have reached the total limit.
        offsetHeader = "100";
        response = makeSyncStorageResponse(200, DEFAULT_LMHEADER, offsetHeader, recordsHeader);
        mockDownloader.onFetchCompleted(response, sessionFetchRecordsDelegate, request, DEFAULT_NEWER,
                limit, true, DEFAULT_SORT, DEFAULT_IDS);

        assertEquals(DEFAULT_LMHEADER, mockDownloader.getLastModified());
        assertTrue(sessionFetchRecordsDelegate.isSuccess);
        assertFalse(sessionFetchRecordsDelegate.isFetched);
        assertFalse(sessionFetchRecordsDelegate.isFailure);
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
    public void testFailureMissingLMMultiBatch() throws Exception {
        assertNull(mockDownloader.getLastModified());

        String offsetHeader = "100";
        SyncStorageResponse response = makeSyncStorageResponse(200, null, offsetHeader, null);
        SyncStorageCollectionRequest request = new SyncStorageCollectionRequest(new URI("http://dummy.url"));
        mockDownloader.onFetchCompleted(response, sessionFetchRecordsDelegate, request, DEFAULT_NEWER,
                1, true, DEFAULT_SORT, DEFAULT_IDS);

        // Last modified header somehow missing from response.
        assertNull(null, mockDownloader.getLastModified());
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
        BatchingDownloader downloader = new BatchingDownloader(serverRepository, mockRepositorySession);
        assertFalse(mockRepositorySession.abort);
        downloader.abortRequests();
        assertTrue(mockRepositorySession.abort);
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
