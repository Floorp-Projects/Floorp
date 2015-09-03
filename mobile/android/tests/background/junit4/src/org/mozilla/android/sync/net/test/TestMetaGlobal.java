/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.net.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;

import org.json.simple.parser.ParseException;
import org.junit.Before;
import org.junit.Test;
import org.mozilla.android.sync.test.helpers.HTTPServerTestHelper;
import org.mozilla.android.sync.test.helpers.MockServer;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.MetaGlobal;
import org.mozilla.gecko.sync.delegates.MetaGlobalDelegate;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.BasicAuthHeaderProvider;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.simpleframework.http.Request;
import org.simpleframework.http.Response;

public class TestMetaGlobal {
  public static Object monitor = new Object();

  private static final int    TEST_PORT    = HTTPServerTestHelper.getTestPort();
  private static final String TEST_SERVER  = "http://localhost:" + TEST_PORT;
  private static final String TEST_SYNC_ID = "foobar";

  public static final String USER_PASS = "c6o7dvmr2c4ud2fyv6woz2u4zi22bcyd:password";
  public static final String META_URL  = TEST_SERVER + "/1.1/c6o7dvmr2c4ud2fyv6woz2u4zi22bcyd/storage/meta/global";
  private HTTPServerTestHelper data    = new HTTPServerTestHelper();


  public static final String TEST_DECLINED_META_GLOBAL_RESPONSE =
          "{\"id\":\"global\"," +
          "\"payload\":" +
          "\"{\\\"syncID\\\":\\\"zPSQTm7WBVWB\\\"," +
          "\\\"declined\\\":[\\\"bookmarks\\\"]," +
          "\\\"storageVersion\\\":5," +
          "\\\"engines\\\":{" +
          "\\\"clients\\\":{\\\"version\\\":1,\\\"syncID\\\":\\\"fDg0MS5bDtV7\\\"}," +
          "\\\"forms\\\":{\\\"version\\\":1,\\\"syncID\\\":\\\"GXF29AFprnvc\\\"}," +
          "\\\"history\\\":{\\\"version\\\":1,\\\"syncID\\\":\\\"av75g4vm-_rp\\\"}," +
          "\\\"passwords\\\":{\\\"version\\\":1,\\\"syncID\\\":\\\"LT_ACGpuKZ6a\\\"}," +
          "\\\"prefs\\\":{\\\"version\\\":2,\\\"syncID\\\":\\\"-3nsksP9wSAs\\\"}," +
          "\\\"tabs\\\":{\\\"version\\\":1,\\\"syncID\\\":\\\"W4H5lOMChkYA\\\"}}}\"," +
          "\"username\":\"5817483\"," +
          "\"modified\":1.32046073744E9}";

  public static final String TEST_META_GLOBAL_RESPONSE =
          "{\"id\":\"global\"," +
          "\"payload\":" +
          "\"{\\\"syncID\\\":\\\"zPSQTm7WBVWB\\\"," +
          "\\\"storageVersion\\\":5," +
          "\\\"engines\\\":{" +
          "\\\"clients\\\":{\\\"version\\\":1,\\\"syncID\\\":\\\"fDg0MS5bDtV7\\\"}," +
          "\\\"bookmarks\\\":{\\\"version\\\":2,\\\"syncID\\\":\\\"NNaQr6_F-9dm\\\"}," +
          "\\\"forms\\\":{\\\"version\\\":1,\\\"syncID\\\":\\\"GXF29AFprnvc\\\"}," +
          "\\\"history\\\":{\\\"version\\\":1,\\\"syncID\\\":\\\"av75g4vm-_rp\\\"}," +
          "\\\"passwords\\\":{\\\"version\\\":1,\\\"syncID\\\":\\\"LT_ACGpuKZ6a\\\"}," +
          "\\\"prefs\\\":{\\\"version\\\":2,\\\"syncID\\\":\\\"-3nsksP9wSAs\\\"}," +
          "\\\"tabs\\\":{\\\"version\\\":1,\\\"syncID\\\":\\\"W4H5lOMChkYA\\\"}}}\"," +
          "\"username\":\"5817483\"," +
          "\"modified\":1.32046073744E9}";
  public static final String TEST_META_GLOBAL_NO_PAYLOAD_RESPONSE = "{\"id\":\"global\"," +
      "\"username\":\"5817483\",\"modified\":1.32046073744E9}";
  public static final String TEST_META_GLOBAL_MALFORMED_PAYLOAD_RESPONSE = "{\"id\":\"global\"," +
      "\"payload\":\"{!!!}\"," +
      "\"username\":\"5817483\",\"modified\":1.32046073744E9}";
  public static final String TEST_META_GLOBAL_EMPTY_PAYLOAD_RESPONSE = "{\"id\":\"global\"," +
      "\"payload\":\"{}\"," +
      "\"username\":\"5817483\",\"modified\":1.32046073744E9}";

  public MetaGlobal global;

  @SuppressWarnings("static-method")
  @Before
  public void setUp() {
    BaseResource.rewriteLocalhost = false;
    global = new MetaGlobal(META_URL, new BasicAuthHeaderProvider(USER_PASS));
  }

  @SuppressWarnings("static-method")
  @Test
  public void testSyncID() {
    global.setSyncID("foobar");
    assertEquals(global.getSyncID(), "foobar");
  }

  public class MockMetaGlobalFetchDelegate implements MetaGlobalDelegate {
    boolean             successCalled   = false;
    MetaGlobal          successGlobal   = null;
    SyncStorageResponse successResponse = null;
    boolean             failureCalled   = false;
    SyncStorageResponse failureResponse = null;
    boolean             errorCalled     = false;
    Exception           errorException  = null;
    boolean             missingCalled   = false;
    MetaGlobal          missingGlobal   = null;
    SyncStorageResponse missingResponse = null;

    public void handleSuccess(MetaGlobal global, SyncStorageResponse response) {
      successCalled = true;
      successGlobal = global;
      successResponse = response;
      WaitHelper.getTestWaiter().performNotify();
    }

    public void handleFailure(SyncStorageResponse response) {
      failureCalled = true;
      failureResponse = response;
      WaitHelper.getTestWaiter().performNotify();
    }

    public void handleError(Exception e) {
      errorCalled = true;
      errorException = e;
      WaitHelper.getTestWaiter().performNotify();
    }

    public void handleMissing(MetaGlobal global, SyncStorageResponse response) {
      missingCalled = true;
      missingGlobal = global;
      missingResponse = response;
      WaitHelper.getTestWaiter().performNotify();
    }
  }

  public MockMetaGlobalFetchDelegate doFetch(final MetaGlobal global) {
    final MockMetaGlobalFetchDelegate delegate = new MockMetaGlobalFetchDelegate();
    WaitHelper.getTestWaiter().performWait(WaitHelper.onThreadRunnable(new Runnable() {
      @Override
      public void run() {
        global.fetch(delegate);
      }
    }));

    return delegate;
  }

  @Test
  public void testFetchMissing() {
    MockServer missingMetaGlobalServer = new MockServer(404, "{}");
    global.setSyncID(TEST_SYNC_ID);
    assertEquals(TEST_SYNC_ID, global.getSyncID());

    data.startHTTPServer(missingMetaGlobalServer);
    final MockMetaGlobalFetchDelegate delegate = doFetch(global);
    data.stopHTTPServer();

    assertTrue(delegate.missingCalled);
    assertEquals(404, delegate.missingResponse.getStatusCode());
    assertEquals(TEST_SYNC_ID, delegate.missingGlobal.getSyncID());
  }

  @Test
  public void testFetchExisting() {
    MockServer existingMetaGlobalServer = new MockServer(200, TEST_META_GLOBAL_RESPONSE);
    assertNull(global.getSyncID());
    assertNull(global.getEngines());
    assertNull(global.getStorageVersion());

    data.startHTTPServer(existingMetaGlobalServer);
    final MockMetaGlobalFetchDelegate delegate = doFetch(global);
    data.stopHTTPServer();

    assertTrue(delegate.successCalled);
    assertEquals(200, delegate.successResponse.getStatusCode());
    assertEquals("zPSQTm7WBVWB", global.getSyncID());
    assertTrue(global.getEngines() instanceof ExtendedJSONObject);
    assertEquals(Long.valueOf(5), global.getStorageVersion());
  }

  /**
   * A record that is valid JSON but invalid as a meta/global record will be
   * downloaded successfully, but will fail later.
   */
  @Test
  public void testFetchNoPayload() {
    MockServer existingMetaGlobalServer = new MockServer(200, TEST_META_GLOBAL_NO_PAYLOAD_RESPONSE);

    data.startHTTPServer(existingMetaGlobalServer);
    final MockMetaGlobalFetchDelegate delegate = doFetch(global);
    data.stopHTTPServer();

    assertTrue(delegate.successCalled);
  }

  @Test
  public void testFetchEmptyPayload() {
    MockServer existingMetaGlobalServer = new MockServer(200, TEST_META_GLOBAL_EMPTY_PAYLOAD_RESPONSE);

    data.startHTTPServer(existingMetaGlobalServer);
    final MockMetaGlobalFetchDelegate delegate = doFetch(global);
    data.stopHTTPServer();

    assertTrue(delegate.successCalled);
  }

  /**
   * A record that is invalid JSON will fail to download at all.
   */
  @Test
  public void testFetchMalformedPayload() {
    MockServer existingMetaGlobalServer = new MockServer(200, TEST_META_GLOBAL_MALFORMED_PAYLOAD_RESPONSE);

    data.startHTTPServer(existingMetaGlobalServer);
    final MockMetaGlobalFetchDelegate delegate = doFetch(global);
    data.stopHTTPServer();

    assertTrue(delegate.errorCalled);
    assertNotNull(delegate.errorException);
    assertEquals(ParseException.class, delegate.errorException.getClass());
  }

  @SuppressWarnings("static-method")
  @Test
  public void testSetFromRecord() throws Exception {
    MetaGlobal mg = new MetaGlobal(null, null);
    mg.setFromRecord(CryptoRecord.fromJSONRecord(TEST_META_GLOBAL_RESPONSE));
    assertEquals("zPSQTm7WBVWB", mg.getSyncID());
    assertTrue(mg.getEngines() instanceof ExtendedJSONObject);
    assertEquals(Long.valueOf(5), mg.getStorageVersion());
  }

  @SuppressWarnings("static-method")
  @Test
  public void testAsCryptoRecord() throws Exception {
    MetaGlobal mg = new MetaGlobal(null, null);
    mg.setFromRecord(CryptoRecord.fromJSONRecord(TEST_META_GLOBAL_RESPONSE));
    CryptoRecord rec = mg.asCryptoRecord();
    assertEquals("global", rec.guid);
    mg.setFromRecord(rec);
    assertEquals("zPSQTm7WBVWB", mg.getSyncID());
    assertTrue(mg.getEngines() instanceof ExtendedJSONObject);
    assertEquals(Long.valueOf(5), mg.getStorageVersion());
  }

  @SuppressWarnings("static-method")
  @Test
  public void testGetEnabledEngineNames() throws Exception {
    MetaGlobal mg = new MetaGlobal(null, null);
    mg.setFromRecord(CryptoRecord.fromJSONRecord(TEST_META_GLOBAL_RESPONSE));
    assertEquals("zPSQTm7WBVWB", mg.getSyncID());
    final Set<String> actual = mg.getEnabledEngineNames();
    final Set<String> expected = new HashSet<String>();
    for (String name : new String[] { "bookmarks", "clients", "forms", "history", "passwords", "prefs", "tabs" }) {
      expected.add(name);
    }
    assertEquals(expected, actual);
  }

  @SuppressWarnings("static-method")
  @Test
  public void testGetEmptyDeclinedEngineNames() throws Exception {
    MetaGlobal mg = new MetaGlobal(null, null);
    mg.setFromRecord(CryptoRecord.fromJSONRecord(TEST_META_GLOBAL_RESPONSE));
    assertEquals(0, mg.getDeclinedEngineNames().size());
  }

  @SuppressWarnings("static-method")
  @Test
  public void testGetDeclinedEngineNames() throws Exception {
    MetaGlobal mg = new MetaGlobal(null, null);
    mg.setFromRecord(CryptoRecord.fromJSONRecord(TEST_DECLINED_META_GLOBAL_RESPONSE));
    assertEquals(1, mg.getDeclinedEngineNames().size());
    assertEquals("bookmarks", mg.getDeclinedEngineNames().iterator().next());
  }

  @SuppressWarnings("static-method")
  @Test
  public void testRoundtripDeclinedEngineNames() throws Exception {
    MetaGlobal mg = new MetaGlobal(null, null);
    mg.setFromRecord(CryptoRecord.fromJSONRecord(TEST_DECLINED_META_GLOBAL_RESPONSE));
    assertEquals("bookmarks", mg.getDeclinedEngineNames().iterator().next());
    assertEquals("bookmarks", mg.asCryptoRecord().payload.getArray("declined").get(0));
    MetaGlobal again = new MetaGlobal(null, null);
    again.setFromRecord(mg.asCryptoRecord());
    assertEquals("bookmarks", again.getDeclinedEngineNames().iterator().next());
  }


  public MockMetaGlobalFetchDelegate doUpload(final MetaGlobal global) {
    final MockMetaGlobalFetchDelegate delegate = new MockMetaGlobalFetchDelegate();
    WaitHelper.getTestWaiter().performWait(WaitHelper.onThreadRunnable(new Runnable() {
      @Override
      public void run() {
        global.upload(delegate);
      }
    }));

    return delegate;
  }

  @Test
  public void testUpload() {
    long TEST_STORAGE_VERSION = 111;
    String TEST_SYNC_ID = "testSyncID";
    global.setSyncID(TEST_SYNC_ID);
    global.setStorageVersion(Long.valueOf(TEST_STORAGE_VERSION));

    final AtomicBoolean mgUploaded = new AtomicBoolean(false);
    final MetaGlobal uploadedMg = new MetaGlobal(null, null);

    MockServer server = new MockServer() {
      public void handle(Request request, Response response) {
        if (request.getMethod().equals("PUT")) {
          try {
            ExtendedJSONObject body = ExtendedJSONObject.parseJSONObject(request.getContent());
            System.out.println(body.toJSONString());
            assertTrue(body.containsKey("payload"));
            assertFalse(body.containsKey("default"));

            CryptoRecord rec = CryptoRecord.fromJSONRecord(body);
            uploadedMg.setFromRecord(rec);
            mgUploaded.set(true);
          } catch (Exception e) {
            throw new RuntimeException(e);
          }
          this.handle(request, response, 200, "success");
          return;
        }
        this.handle(request, response, 404, "missing");
      }
    };

    data.startHTTPServer(server);
    final MockMetaGlobalFetchDelegate delegate = doUpload(global);
    data.stopHTTPServer();

    assertTrue(delegate.successCalled);
    assertTrue(mgUploaded.get());
    assertEquals(TEST_SYNC_ID, uploadedMg.getSyncID());
    assertEquals(TEST_STORAGE_VERSION, uploadedMg.getStorageVersion().longValue());
  }
}
