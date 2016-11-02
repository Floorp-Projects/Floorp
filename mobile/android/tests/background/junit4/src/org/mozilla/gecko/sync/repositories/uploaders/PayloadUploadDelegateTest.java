/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.repositories.uploaders;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.SyncResponse;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

import static org.mockito.Mockito.mock;

import java.io.ByteArrayInputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.concurrent.Executor;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.entity.BasicHttpEntity;
import ch.boye.httpclientandroidlib.message.BasicHttpResponse;
import ch.boye.httpclientandroidlib.message.BasicStatusLine;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class PayloadUploadDelegateTest {
    private PayloadDispatcher payloadDispatcher;
    private AuthHeaderProvider authHeaderProvider;

    class MockPayloadDispatcher extends PayloadDispatcher {
        public final HashMap<String, Exception> failedRecords = new HashMap<>();
        public boolean didLastPayloadFail = false;

        public ArrayList<SyncStorageResponse> successResponses = new ArrayList<>();
        public int commitPayloadsSucceeded = 0;
        public int lastPayloadsSucceeded = 0;

        public int committedGuids = 0;

        public MockPayloadDispatcher(final Executor workQueue, final BatchingUploader uploader) {
            super(workQueue, uploader, null);
        }

        @Override
        public void payloadSucceeded(final SyncStorageResponse response, String[] guids, final boolean isCommit, final boolean isLastPayload) {
            successResponses.add(response);
            if (isCommit) {
                ++commitPayloadsSucceeded;
                committedGuids += guids.length;
            }
            if (isLastPayload) {
                ++lastPayloadsSucceeded;
            }
        }

        @Override
        public void recordFailed(final String recordGuid) {
            recordFailed(new Exception(), recordGuid);
        }

        @Override
        public void recordFailed(final Exception e, final String recordGuid) {
            failedRecords.put(recordGuid, e);
        }

        @Override
        public void lastPayloadFailed() {
            didLastPayloadFail = true;
        }
    }

    @Before
    public void setUp() throws Exception {
        payloadDispatcher = new MockPayloadDispatcher(
                null,
                mock(BatchingUploader.class)
        );

        authHeaderProvider = mock(AuthHeaderProvider.class);
    }

    @Test
    public void testHandleRequestSuccessNonSuccess() {
        ArrayList<String> postedGuids = new ArrayList<>(2);
        postedGuids.add("testGuid1");
        postedGuids.add("testGuid2");
        PayloadUploadDelegate payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, postedGuids, false, false);

        // Test that non-2* responses aren't processed
        payloadUploadDelegate.handleRequestSuccess(makeSyncStorageResponse(404, null, null));
        assertEquals(2, ((MockPayloadDispatcher) payloadDispatcher).failedRecords.size());
        assertFalse(((MockPayloadDispatcher) payloadDispatcher).didLastPayloadFail);
        assertEquals(IllegalStateException.class,
                ((MockPayloadDispatcher) payloadDispatcher).failedRecords.get("testGuid1").getClass());
        assertEquals(IllegalStateException.class,
                ((MockPayloadDispatcher) payloadDispatcher).failedRecords.get("testGuid2").getClass());
    }

    @Test
    public void testHandleRequestSuccessNoHeaders() {
        ArrayList<String> postedGuids = new ArrayList<>(2);
        postedGuids.add("testGuid1");
        postedGuids.add("testGuid2");
        PayloadUploadDelegate payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, postedGuids, false, false);

        // Test that responses without X-Last-Modified header aren't processed
        payloadUploadDelegate.handleRequestSuccess(makeSyncStorageResponse(200, null, null));
        assertEquals(2, ((MockPayloadDispatcher) payloadDispatcher).failedRecords.size());
        assertFalse(((MockPayloadDispatcher) payloadDispatcher).didLastPayloadFail);
        assertEquals(IllegalStateException.class,
                ((MockPayloadDispatcher) payloadDispatcher).failedRecords.get("testGuid1").getClass());
        assertEquals(IllegalStateException.class,
                ((MockPayloadDispatcher) payloadDispatcher).failedRecords.get("testGuid2").getClass());
    }

    @Test
    public void testHandleRequestSuccessBadBody() {
        ArrayList<String> postedGuids = new ArrayList<>(2);
        postedGuids.add("testGuid1");
        postedGuids.add("testGuid2");
        PayloadUploadDelegate payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, postedGuids, false, true);

        // Test that we catch json processing errors
        payloadUploadDelegate.handleRequestSuccess(makeSyncStorageResponse(200, "non json body", "123"));
        assertEquals(2, ((MockPayloadDispatcher) payloadDispatcher).failedRecords.size());
        assertTrue(((MockPayloadDispatcher) payloadDispatcher).didLastPayloadFail);
        assertEquals(NonObjectJSONException.class,
                ((MockPayloadDispatcher) payloadDispatcher).failedRecords.get("testGuid1").getClass());
        assertEquals(NonObjectJSONException.class,
                ((MockPayloadDispatcher) payloadDispatcher).failedRecords.get("testGuid2").getClass());
    }

    @Test
    public void testHandleRequestSuccess202NoToken() {
        ArrayList<String> postedGuids = new ArrayList<>(1);
        postedGuids.add("testGuid1");
        PayloadUploadDelegate payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, postedGuids, false, true);

        // Test that we catch absent tokens in 202 responses
        payloadUploadDelegate.handleRequestSuccess(makeSyncStorageResponse(202, "{\"success\": []}", "123"));
        assertEquals(1, ((MockPayloadDispatcher) payloadDispatcher).failedRecords.size());
        assertEquals(IllegalStateException.class,
                ((MockPayloadDispatcher) payloadDispatcher).failedRecords.get("testGuid1").getClass());
    }

    @Test
    public void testHandleRequestSuccessBad200() {
        ArrayList<String> postedGuids = new ArrayList<>(1);
        postedGuids.add("testGuid1");

        PayloadUploadDelegate payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, postedGuids, false, false);

        // Test that if in batching mode and saw the token, 200 must be a response to a commit
        try {
            payloadDispatcher.batchWhiteboard.setToken("MTIzNA", true);
        } catch (BatchingUploader.BatchingUploaderException e) {}
        payloadDispatcher.setInBatchingMode(true);

        // not a commit, so should fail
        payloadUploadDelegate.handleRequestSuccess(makeSyncStorageResponse(200, "{\"success\": []}", "123"));
        assertEquals(1, ((MockPayloadDispatcher) payloadDispatcher).failedRecords.size());
        assertEquals(IllegalStateException.class,
                ((MockPayloadDispatcher) payloadDispatcher).failedRecords.get("testGuid1").getClass());
    }

    @Test
    public void testHandleRequestSuccessNonBatchingFailedLM() {
        ArrayList<String> postedGuids = new ArrayList<>(1);
        postedGuids.add("guid1");
        postedGuids.add("guid2");
        postedGuids.add("guid3");
        PayloadUploadDelegate payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, postedGuids, false, false);

        payloadUploadDelegate.handleRequestSuccess(
                makeSyncStorageResponse(200, "{\"success\": [\"guid1\", \"guid2\", \"guid3\"]}", "123"));
        assertEquals(0, ((MockPayloadDispatcher) payloadDispatcher).failedRecords.size());
        assertEquals(3, payloadDispatcher.batchWhiteboard.getSuccessRecordGuids().length);
        assertFalse(((MockPayloadDispatcher) payloadDispatcher).didLastPayloadFail);
        assertEquals(1, ((MockPayloadDispatcher) payloadDispatcher).successResponses.size());
        assertEquals(0, ((MockPayloadDispatcher) payloadDispatcher).commitPayloadsSucceeded);
        assertEquals(0, ((MockPayloadDispatcher) payloadDispatcher).lastPayloadsSucceeded);

        // These should fail, because we're returning a non-changed L-M in a non-batching mode
        postedGuids.add("guid4");
        postedGuids.add("guid6");
        payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, postedGuids, false, false);
        payloadUploadDelegate.handleRequestSuccess(
                makeSyncStorageResponse(200, "{\"success\": [\"guid4\", 5, \"guid6\"]}", "123"));
        assertEquals(5, ((MockPayloadDispatcher) payloadDispatcher).failedRecords.size());
        assertEquals(3, payloadDispatcher.batchWhiteboard.getSuccessRecordGuids().length);
        assertFalse(((MockPayloadDispatcher) payloadDispatcher).didLastPayloadFail);
        assertEquals(1, ((MockPayloadDispatcher) payloadDispatcher).successResponses.size());
        assertEquals(0, ((MockPayloadDispatcher) payloadDispatcher).commitPayloadsSucceeded);
        assertEquals(0, ((MockPayloadDispatcher) payloadDispatcher).lastPayloadsSucceeded);
        assertEquals(BatchingUploader.LastModifiedDidNotChange.class,
                ((MockPayloadDispatcher) payloadDispatcher).failedRecords.get("guid4").getClass());
    }

    @Test
    public void testHandleRequestSuccessNonBatching() {
        ArrayList<String> postedGuids = new ArrayList<>();
        postedGuids.add("guid1");
        postedGuids.add("guid2");
        postedGuids.add("guid3");
        PayloadUploadDelegate payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, postedGuids, false, false);
        payloadUploadDelegate.handleRequestSuccess(
                makeSyncStorageResponse(200, "{\"success\": [\"guid1\", \"guid2\", \"guid3\"], \"failed\": {}}", "123"));

        postedGuids = new ArrayList<>();
        postedGuids.add("guid4");
        postedGuids.add("guid5");
        payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, postedGuids, false, false);
        payloadUploadDelegate.handleRequestSuccess(
                makeSyncStorageResponse(200, "{\"success\": [\"guid4\", \"guid5\"], \"failed\": {}}", "333"));

        postedGuids = new ArrayList<>();
        postedGuids.add("guid6");
        payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, postedGuids, false, true);
        payloadUploadDelegate.handleRequestSuccess(
                makeSyncStorageResponse(200, "{\"success\": [\"guid6\"], \"failed\": {}}", "444"));

        assertEquals(0, ((MockPayloadDispatcher) payloadDispatcher).failedRecords.size());
        assertEquals(6, payloadDispatcher.batchWhiteboard.getSuccessRecordGuids().length);
        assertFalse(((MockPayloadDispatcher) payloadDispatcher).didLastPayloadFail);
        assertEquals(3, ((MockPayloadDispatcher) payloadDispatcher).successResponses.size());
        assertEquals(0, ((MockPayloadDispatcher) payloadDispatcher).commitPayloadsSucceeded);
        assertEquals(1, ((MockPayloadDispatcher) payloadDispatcher).lastPayloadsSucceeded);
        assertFalse(payloadDispatcher.batchWhiteboard.getInBatchingMode());

        postedGuids = new ArrayList<>();
        postedGuids.add("guid7");
        postedGuids.add("guid8");
        payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, postedGuids, false, true);
        payloadUploadDelegate.handleRequestSuccess(
                makeSyncStorageResponse(200, "{\"success\": [\"guid8\"], \"failed\": {\"guid7\": \"reason\"}}", "555"));
        assertEquals(1, ((MockPayloadDispatcher) payloadDispatcher).failedRecords.size());
        assertTrue(((MockPayloadDispatcher) payloadDispatcher).failedRecords.containsKey("guid7"));
        assertEquals(7, payloadDispatcher.batchWhiteboard.getSuccessRecordGuids().length);
        assertFalse(((MockPayloadDispatcher) payloadDispatcher).didLastPayloadFail);
        assertEquals(4, ((MockPayloadDispatcher) payloadDispatcher).successResponses.size());
        assertEquals(0, ((MockPayloadDispatcher) payloadDispatcher).commitPayloadsSucceeded);
        assertEquals(2, ((MockPayloadDispatcher) payloadDispatcher).lastPayloadsSucceeded);
        assertFalse(payloadDispatcher.batchWhiteboard.getInBatchingMode());
    }

    @Test
    public void testHandleRequestSuccessBatching() {
        assertNull(payloadDispatcher.batchWhiteboard.getInBatchingMode());

        ArrayList<String> postedGuids = new ArrayList<>();
        postedGuids.add("guid1");
        postedGuids.add("guid2");
        postedGuids.add("guid3");
        PayloadUploadDelegate payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, postedGuids, false, false);
        payloadUploadDelegate.handleRequestSuccess(
                makeSyncStorageResponse(202, "{\"batch\": \"MTIzNA\", \"success\": [\"guid1\", \"guid2\", \"guid3\"], \"failed\": {}}", "123"));

        assertTrue(payloadDispatcher.batchWhiteboard.getInBatchingMode());
        assertEquals("MTIzNA", payloadDispatcher.batchWhiteboard.getToken());

        postedGuids = new ArrayList<>();
        postedGuids.add("guid4");
        postedGuids.add("guid5");
        postedGuids.add("guid6");
        payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, postedGuids, false, false);
        payloadUploadDelegate.handleRequestSuccess(
                makeSyncStorageResponse(202, "{\"batch\": \"MTIzNA\", \"success\": [\"guid4\", \"guid5\", \"guid6\"], \"failed\": {}}", "123"));

        assertTrue(payloadDispatcher.batchWhiteboard.getInBatchingMode());
        assertEquals("MTIzNA", payloadDispatcher.batchWhiteboard.getToken());

        postedGuids = new ArrayList<>();
        postedGuids.add("guid7");
        payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, postedGuids, true, false);
        payloadUploadDelegate.handleRequestSuccess(
                makeSyncStorageResponse(200, "{\"success\": [\"guid7\"], \"failed\": {}}", "222"));

        assertEquals(7, ((MockPayloadDispatcher) payloadDispatcher).committedGuids);

        // Even though everything indicates we're not in a batching, we were, so test that
        // we don't reset the flag.
        assertTrue(payloadDispatcher.batchWhiteboard.getInBatchingMode());
        assertNull(payloadDispatcher.batchWhiteboard.getToken());

        postedGuids = new ArrayList<>();
        postedGuids.add("guid8");
        payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, postedGuids, true, true);
        payloadUploadDelegate.handleRequestSuccess(
                makeSyncStorageResponse(200, "{\"success\": [\"guid7\"], \"failed\": {}}", "333"));

        assertEquals(0, ((MockPayloadDispatcher) payloadDispatcher).failedRecords.size());
        assertEquals(8, ((MockPayloadDispatcher) payloadDispatcher).committedGuids);
        assertFalse(((MockPayloadDispatcher) payloadDispatcher).didLastPayloadFail);
        assertEquals(4, ((MockPayloadDispatcher) payloadDispatcher).successResponses.size());
        assertEquals(2, ((MockPayloadDispatcher) payloadDispatcher).commitPayloadsSucceeded);
        assertEquals(1, ((MockPayloadDispatcher) payloadDispatcher).lastPayloadsSucceeded);
        assertTrue(payloadDispatcher.batchWhiteboard.getInBatchingMode());
    }

    @Test
    public void testHandleRequestError() {
        ArrayList<String> postedGuids = new ArrayList<>(3);
        postedGuids.add("testGuid1");
        postedGuids.add("testGuid2");
        postedGuids.add("testGuid3");
        PayloadUploadDelegate payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, postedGuids, false, false);

        IllegalStateException e = new IllegalStateException();
        payloadUploadDelegate.handleRequestError(e);

        assertEquals(3, ((MockPayloadDispatcher) payloadDispatcher).failedRecords.size());
        assertEquals(e, ((MockPayloadDispatcher) payloadDispatcher).failedRecords.get("testGuid1"));
        assertEquals(e, ((MockPayloadDispatcher) payloadDispatcher).failedRecords.get("testGuid2"));
        assertEquals(e, ((MockPayloadDispatcher) payloadDispatcher).failedRecords.get("testGuid3"));
        assertFalse(((MockPayloadDispatcher) payloadDispatcher).didLastPayloadFail);

        payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, postedGuids, false, true);
        payloadUploadDelegate.handleRequestError(e);
        assertEquals(3, ((MockPayloadDispatcher) payloadDispatcher).failedRecords.size());
        assertTrue(((MockPayloadDispatcher) payloadDispatcher).didLastPayloadFail);
    }

    @Test
    public void testHandleRequestFailure() {
        ArrayList<String> postedGuids = new ArrayList<>(3);
        postedGuids.add("testGuid1");
        postedGuids.add("testGuid2");
        postedGuids.add("testGuid3");
        PayloadUploadDelegate payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, postedGuids, false, false);

        final HttpResponse response = new BasicHttpResponse(
                new BasicStatusLine(new ProtocolVersion("HTTP", 1, 1), 503, "Illegal method/protocol"));
        payloadUploadDelegate.handleRequestFailure(new SyncStorageResponse(response));
        assertEquals(3, ((MockPayloadDispatcher) payloadDispatcher).failedRecords.size());
        assertEquals(HTTPFailureException.class,
                ((MockPayloadDispatcher) payloadDispatcher).failedRecords.get("testGuid1").getClass());
        assertEquals(HTTPFailureException.class,
                ((MockPayloadDispatcher) payloadDispatcher).failedRecords.get("testGuid2").getClass());
        assertEquals(HTTPFailureException.class,
                ((MockPayloadDispatcher) payloadDispatcher).failedRecords.get("testGuid3").getClass());

        payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, postedGuids, false, true);
        payloadUploadDelegate.handleRequestFailure(new SyncStorageResponse(response));
        assertEquals(3, ((MockPayloadDispatcher) payloadDispatcher).failedRecords.size());
        assertTrue(((MockPayloadDispatcher) payloadDispatcher).didLastPayloadFail);
    }

    @Test
    public void testIfUnmodifiedSinceNoLM() {
        PayloadUploadDelegate payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, new ArrayList<String>(), false, false);

        assertNull(payloadUploadDelegate.ifUnmodifiedSince());
    }

    @Test
    public void testIfUnmodifiedSinceWithLM() {
        PayloadUploadDelegate payloadUploadDelegate = new PayloadUploadDelegate(
                authHeaderProvider, payloadDispatcher, new ArrayList<String>(), false, false);
        try {
            payloadDispatcher.batchWhiteboard.setLastModified(1471645412480L, true);
        } catch (BatchingUploader.BatchingUploaderException e) {
            fail();
        }

        assertEquals("1471645412.480", payloadUploadDelegate.ifUnmodifiedSince());
    }

    private SyncStorageResponse makeSyncStorageResponse(int code, String body, String lastModified) {
        BasicHttpResponse response = new BasicHttpResponse(
                new BasicStatusLine(new ProtocolVersion("HTTP", 1, 1), code, null));

        if (body != null) {
            BasicHttpEntity entity = new BasicHttpEntity();
            entity.setContent(new ByteArrayInputStream(body.getBytes()));
            response.setEntity(entity);
        }

        if (lastModified != null) {
            response.addHeader(SyncResponse.X_LAST_MODIFIED, lastModified);
        }
        return new SyncStorageResponse(response);
    }
}