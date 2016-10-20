/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.downloaders;

import android.net.Uri;
import android.os.SystemClock;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.InfoCollections;
import org.mozilla.gecko.sync.InfoConfiguration;
import org.mozilla.gecko.sync.net.SyncResponse;
import org.mozilla.gecko.sync.net.SyncStorageCollectionRequest;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.Server11RepositorySession;
import org.mozilla.gecko.sync.repositories.Server11Repository;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

import java.net.URI;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.TimeUnit;

import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.message.BasicHttpResponse;
import ch.boye.httpclientandroidlib.message.BasicStatusLine;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class BatchingDownloaderDelegateTest {
    private Server11RepositorySession repositorySession;
    private MockDownloader mockDownloader;
    private String DEFAULT_COLLECTION_URL = "http://dummy.url/";

    class MockDownloader extends BatchingDownloader {
        public boolean isSuccess = false;
        public boolean isFetched = false;
        public boolean isFailure = false;
        public Exception ex;

        public MockDownloader(RepositorySession repositorySession) {
            super(
                    null,
                    Uri.EMPTY,
                    SystemClock.elapsedRealtime() + TimeUnit.MINUTES.toMillis(30),
                    true,
                    repositorySession
            );
        }

        @Override
        public void onFetchCompleted(SyncStorageResponse response,
                                     final RepositorySessionFetchRecordsDelegate fetchRecordsDelegate,
                                     final SyncStorageCollectionRequest request,
                                     long l, long newerTimestamp, boolean full, String sort, String ids) {
            this.isSuccess = true;
        }

        @Override
        public void onFetchFailed(final Exception ex,
                                  final RepositorySessionFetchRecordsDelegate fetchRecordsDelegate,
                                  final SyncStorageCollectionRequest request) {
            this.isFailure = true;
            this.ex = ex;
        }

        @Override
        public void onFetchedRecord(CryptoRecord record,
                                    RepositorySessionFetchRecordsDelegate fetchRecordsDelegate) {
            this.isFetched = true;
        }
    }

    class SimpleSessionFetchRecordsDelegate implements RepositorySessionFetchRecordsDelegate {
        @Override
        public void onFetchFailed(Exception ex) {

        }

        @Override
        public void onFetchedRecord(Record record) {

        }

        @Override
        public void onFetchCompleted(long fetchEnd) {

        }

        @Override
        public void onBatchCompleted() {

        }

        @Override
        public RepositorySessionFetchRecordsDelegate deferredFetchDelegate(ExecutorService executor) {
            return null;
        }
    }

    @Before
    public void setUp() throws Exception {
        repositorySession = new Server11RepositorySession(new Server11Repository(
                "dummyCollection",
                SystemClock.elapsedRealtime() + TimeUnit.MINUTES.toMillis(30),
                DEFAULT_COLLECTION_URL,
                null,
                new InfoCollections(),
                new InfoConfiguration())
        );
        mockDownloader = new MockDownloader(repositorySession);
    }

    @Test
    public void testIfUnmodifiedSince() throws Exception {
        BatchingDownloader downloader = new BatchingDownloader(
                null,
                Uri.EMPTY,
                SystemClock.elapsedRealtime() + TimeUnit.MINUTES.toMillis(30),
                true,
                repositorySession
        );
        RepositorySessionFetchRecordsDelegate delegate = new SimpleSessionFetchRecordsDelegate();
        BatchingDownloaderDelegate downloaderDelegate = new BatchingDownloaderDelegate(downloader, delegate,
                new SyncStorageCollectionRequest(new URI(DEFAULT_COLLECTION_URL)), 0, 0, true, null, null);
        String lastModified = "12345678";
        SyncStorageResponse response = makeSyncStorageResponse(200, lastModified);
        downloaderDelegate.handleRequestSuccess(response);
        assertEquals(lastModified, downloaderDelegate.ifUnmodifiedSince());
    }

    @Test
    public void testSuccess() throws Exception {
        BatchingDownloaderDelegate downloaderDelegate = new BatchingDownloaderDelegate(mockDownloader, null,
                new SyncStorageCollectionRequest(new URI(DEFAULT_COLLECTION_URL)), 0, 0, true, null, null);
        SyncStorageResponse response = makeSyncStorageResponse(200, "12345678");
        downloaderDelegate.handleRequestSuccess(response);
        assertTrue(mockDownloader.isSuccess);
        assertFalse(mockDownloader.isFailure);
        assertFalse(mockDownloader.isFetched);
    }

    @Test
    public void testFailureMissingLMHeader() throws Exception {
        BatchingDownloaderDelegate downloaderDelegate = new BatchingDownloaderDelegate(mockDownloader, null,
                new SyncStorageCollectionRequest(new URI(DEFAULT_COLLECTION_URL)), 0, 0, true, null, null);
        SyncStorageResponse response = makeSyncStorageResponse(200, null);
        downloaderDelegate.handleRequestSuccess(response);
        assertTrue(mockDownloader.isFailure);
        assertEquals(IllegalStateException.class, mockDownloader.ex.getClass());
        assertFalse(mockDownloader.isSuccess);
        assertFalse(mockDownloader.isFetched);
    }

    @Test
    public void testFailureHTTPException() throws Exception {
        BatchingDownloaderDelegate downloaderDelegate = new BatchingDownloaderDelegate(mockDownloader, null,
                new SyncStorageCollectionRequest(new URI(DEFAULT_COLLECTION_URL)), 0, 0, true, null, null);
        SyncStorageResponse response = makeSyncStorageResponse(400, null);
        downloaderDelegate.handleRequestFailure(response);
        assertTrue(mockDownloader.isFailure);
        assertEquals(HTTPFailureException.class, mockDownloader.ex.getClass());
        assertFalse(mockDownloader.isSuccess);
        assertFalse(mockDownloader.isFetched);
    }

    @Test
    public void testFailureRequestError() throws Exception {
        BatchingDownloaderDelegate downloaderDelegate = new BatchingDownloaderDelegate(mockDownloader, null,
                new SyncStorageCollectionRequest(new URI(DEFAULT_COLLECTION_URL)), 0, 0, true, null, null);
        downloaderDelegate.handleRequestError(new ClientProtocolException());
        assertTrue(mockDownloader.isFailure);
        assertEquals(ClientProtocolException.class, mockDownloader.ex.getClass());
        assertFalse(mockDownloader.isSuccess);
        assertFalse(mockDownloader.isFetched);
    }

    @Test
    public void testFetchRecord() throws Exception {
        BatchingDownloaderDelegate downloaderDelegate = new BatchingDownloaderDelegate(mockDownloader, null,
                new SyncStorageCollectionRequest(new URI(DEFAULT_COLLECTION_URL)), 0, 0, true, null, null);
        CryptoRecord record = new CryptoRecord();
        downloaderDelegate.handleWBO(record);
        assertTrue(mockDownloader.isFetched);
        assertFalse(mockDownloader.isSuccess);
        assertFalse(mockDownloader.isFailure);
    }

    private SyncStorageResponse makeSyncStorageResponse(int code, String lastModified) {
        BasicHttpResponse response = new BasicHttpResponse(
                new BasicStatusLine(new ProtocolVersion("HTTP", 1, 1), code, null));

        if (lastModified != null) {
            response.addHeader(SyncResponse.X_LAST_MODIFIED, lastModified);
        }

        return new SyncStorageResponse(response);
    }
}
