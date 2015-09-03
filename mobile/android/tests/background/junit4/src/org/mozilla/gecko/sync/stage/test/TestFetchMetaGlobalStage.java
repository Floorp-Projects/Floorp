/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.stage.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.net.URI;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;

import org.json.simple.JSONArray;
import org.json.simple.parser.ParseException;
import org.junit.Before;
import org.junit.Test;
import org.mozilla.android.sync.net.test.TestMetaGlobal;
import org.mozilla.android.sync.test.helpers.HTTPServerTestHelper;
import org.mozilla.android.sync.test.helpers.MockGlobalSessionCallback;
import org.mozilla.android.sync.test.helpers.MockServer;
import org.mozilla.gecko.background.testhelpers.MockGlobalSession;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.AlreadySyncingException;
import org.mozilla.gecko.sync.CollectionKeys;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.InfoCollections;
import org.mozilla.gecko.sync.MetaGlobal;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.SyncConfigurationException;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.delegates.FreshStartDelegate;
import org.mozilla.gecko.sync.delegates.KeyUploadDelegate;
import org.mozilla.gecko.sync.delegates.WipeServerDelegate;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.stage.FetchMetaGlobalStage;
import org.mozilla.gecko.sync.stage.GlobalSyncStage.Stage;
import org.simpleframework.http.Request;
import org.simpleframework.http.Response;

public class TestFetchMetaGlobalStage {
  @SuppressWarnings("unused")
  private static final String  LOG_TAG          = "TestMetaGlobalStage";

  private static final int     TEST_PORT        = HTTPServerTestHelper.getTestPort();
  private static final String  TEST_SERVER      = "http://localhost:" + TEST_PORT + "/";
  private static final String  TEST_CLUSTER_URL = TEST_SERVER + "cluster/";
  static String                TEST_NW_URL      = TEST_SERVER + "/1.0/c6o7dvmr2c4ud2fyv6woz2u4zi22bcyd/node/weave"; // GET https://server/pathname/version/username/node/weave
  private HTTPServerTestHelper data             = new HTTPServerTestHelper();

  private final String TEST_USERNAME            = "johndoe";
  private final String TEST_PASSWORD            = "password";
  private final String TEST_SYNC_KEY            = "abcdeabcdeabcdeabcdeabcdea";

  private final String TEST_INFO_COLLECTIONS_JSON = "{}";

  private static final String  TEST_SYNC_ID         = "testSyncID";
  private static final long    TEST_STORAGE_VERSION = GlobalSession.STORAGE_VERSION;

  private InfoCollections infoCollections;
  private KeyBundle syncKeyBundle;
  private MockGlobalSessionCallback callback;
  private GlobalSession session;

  private boolean calledRequiresUpgrade = false;
  private boolean calledProcessMissingMetaGlobal = false;
  private boolean calledFreshStart = false;
  private boolean calledWipeServer = false;
  private boolean calledUploadKeys = false;
  private boolean calledResetAllStages = false;

  private static void assertSameContents(JSONArray expected, Set<String> actual) {
    assertEquals(expected.size(), actual.size());
    for (Object o : expected) {
      assertTrue(actual.contains(o));
    }
  }

  @Before
  public void setUp() throws Exception {
    calledRequiresUpgrade = false;
    calledProcessMissingMetaGlobal = false;
    calledFreshStart = false;
    calledWipeServer = false;
    calledUploadKeys = false;
    calledResetAllStages = false;

    // Set info collections to not have crypto.
    infoCollections = new InfoCollections(ExtendedJSONObject.parseJSONObject(TEST_INFO_COLLECTIONS_JSON));

    syncKeyBundle = new KeyBundle(TEST_USERNAME, TEST_SYNC_KEY);
    callback = new MockGlobalSessionCallback(TEST_CLUSTER_URL);
    session = new MockGlobalSession(TEST_USERNAME, TEST_PASSWORD,
      syncKeyBundle, callback) {
      @Override
      protected void prepareStages() {
        super.prepareStages();
        withStage(Stage.fetchMetaGlobal, new FetchMetaGlobalStage());
      }

      @Override
      public void requiresUpgrade() {
        calledRequiresUpgrade = true;
        this.abort(null, "Requires upgrade");
      }

      @Override
      public void processMissingMetaGlobal(MetaGlobal mg) {
        calledProcessMissingMetaGlobal = true;
        this.abort(null, "Missing meta/global");
      }

      // Don't really uploadKeys.
      @Override
      public void uploadKeys(CollectionKeys keys, KeyUploadDelegate keyUploadDelegate) {
        calledUploadKeys = true;
        keyUploadDelegate.onKeysUploaded();
      }

      // On fresh start completed, just stop.
      @Override
      public void freshStart() {
        calledFreshStart = true;
        freshStart(this, new FreshStartDelegate() {
          @Override
          public void onFreshStartFailed(Exception e) {
            WaitHelper.getTestWaiter().performNotify(e);
          }

          @Override
          public void onFreshStart() {
            WaitHelper.getTestWaiter().performNotify();
          }
        });
      }

      // Don't really wipeServer.
      @Override
      protected void wipeServer(final AuthHeaderProvider authHeaderProvider, final WipeServerDelegate wipeDelegate) {
        calledWipeServer = true;
        wipeDelegate.onWiped(System.currentTimeMillis());
      }

      // Don't really resetAllStages.
      @Override
      public void resetAllStages() {
        calledResetAllStages = true;
      }
    };
    session.config.setClusterURL(new URI(TEST_CLUSTER_URL));
    session.config.infoCollections = infoCollections;
  }

  protected void doSession(MockServer server) {
    data.startHTTPServer(server);
    WaitHelper.getTestWaiter().performWait(WaitHelper.onThreadRunnable(new Runnable() {
      @Override
      public void run() {
        try {
          session.start();
        } catch (AlreadySyncingException e) {
          WaitHelper.getTestWaiter().performNotify(e);
        }
      }
    }));
    data.stopHTTPServer();
  }

  @Test
  public void testFetchRequiresUpgrade() throws Exception {
    MetaGlobal mg = new MetaGlobal(null, null);
    mg.setSyncID(TEST_SYNC_ID);
    mg.setStorageVersion(Long.valueOf(TEST_STORAGE_VERSION + 1));

    MockServer server = new MockServer(200, mg.asCryptoRecord().toJSONString());
    doSession(server);

    assertEquals(true, callback.calledError);
    assertTrue(calledRequiresUpgrade);
  }

  @SuppressWarnings("unchecked")
  private JSONArray makeTestDeclinedArray() {
    final JSONArray declined = new JSONArray();
    declined.add("foobar");
    return declined;
  }

  /**
   * Verify that a fetched meta/global with remote syncID == local syncID does
   * not reset.
   *
   * @throws Exception
   */
  @Test
  public void testFetchSuccessWithSameSyncID() throws Exception {
    session.config.syncID = TEST_SYNC_ID;

    MetaGlobal mg = new MetaGlobal(null, null);
    mg.setSyncID(TEST_SYNC_ID);
    mg.setStorageVersion(Long.valueOf(TEST_STORAGE_VERSION));

    // Set declined engines in the server object.
    final JSONArray testingDeclinedEngines = makeTestDeclinedArray();
    mg.setDeclinedEngineNames(testingDeclinedEngines);

    MockServer server = new MockServer(200, mg.asCryptoRecord().toJSONString());
    doSession(server);

    assertTrue(callback.calledSuccess);
    assertFalse(calledProcessMissingMetaGlobal);
    assertFalse(calledResetAllStages);
    assertEquals(TEST_SYNC_ID, session.config.metaGlobal.getSyncID());
    assertEquals(TEST_STORAGE_VERSION, session.config.metaGlobal.getStorageVersion().longValue());
    assertEquals(TEST_SYNC_ID, session.config.syncID);

    // Declined engines propagate from the server meta/global.
    final Set<String> actual = session.config.metaGlobal.getDeclinedEngineNames();
    assertSameContents(testingDeclinedEngines, actual);
  }

  /**
   * Verify that a fetched meta/global with remote syncID != local syncID resets
   * local and retains remote syncID.
   *
   * @throws Exception
   */
  @Test
  public void testFetchSuccessWithDifferentSyncID() throws Exception {
    session.config.syncID = "NOT TEST SYNC ID";

    MetaGlobal mg = new MetaGlobal(null, null);
    mg.setSyncID(TEST_SYNC_ID);
    mg.setStorageVersion(Long.valueOf(TEST_STORAGE_VERSION));

    // Set declined engines in the server object.
    final JSONArray testingDeclinedEngines = makeTestDeclinedArray();
    mg.setDeclinedEngineNames(testingDeclinedEngines);

    MockServer server = new MockServer(200, mg.asCryptoRecord().toJSONString());
    doSession(server);

    assertEquals(true, callback.calledSuccess);
    assertFalse(calledProcessMissingMetaGlobal);
    assertTrue(calledResetAllStages);
    assertEquals(TEST_SYNC_ID, session.config.metaGlobal.getSyncID());
    assertEquals(TEST_STORAGE_VERSION, session.config.metaGlobal.getStorageVersion().longValue());
    assertEquals(TEST_SYNC_ID, session.config.syncID);

    // Declined engines propagate from the server meta/global.
    final Set<String> actual = session.config.metaGlobal.getDeclinedEngineNames();
    assertSameContents(testingDeclinedEngines, actual);
  }

  /**
   * Verify that a fetched meta/global does not merge declined engines.
   * TODO: eventually it should!
   */
  @SuppressWarnings("unchecked")
  @Test
  public void testFetchSuccessWithDifferentSyncIDMergesDeclined() throws Exception {
    session.config.syncID = "NOT TEST SYNC ID";

    // Fake the local declined engine names.
    session.config.declinedEngineNames = new HashSet<String>();
    session.config.declinedEngineNames.add("baznoo");

    MetaGlobal mg = new MetaGlobal(null, null);
    mg.setSyncID(TEST_SYNC_ID);
    mg.setStorageVersion(Long.valueOf(TEST_STORAGE_VERSION));

    // Set declined engines in the server object.
    final JSONArray testingDeclinedEngines = makeTestDeclinedArray();
    mg.setDeclinedEngineNames(testingDeclinedEngines);

    MockServer server = new MockServer(200, mg.asCryptoRecord().toJSONString());
    doSession(server);

    // Declined engines propagate from the server meta/global, and are NOT merged.
    final Set<String> expected = new HashSet<String>(testingDeclinedEngines);
    // expected.add("baznoo");   // Not until we merge. Local is lost.

    final Set<String> newDeclined = session.config.metaGlobal.getDeclinedEngineNames();
    assertEquals(expected, newDeclined);
  }

  @Test
  public void testFetchMissing() throws Exception {
    MockServer server = new MockServer(404, "missing");
    doSession(server);

    assertEquals(true, callback.calledError);
    assertTrue(calledProcessMissingMetaGlobal);
  }

  /**
   * Empty payload object has no syncID or storageVersion and should call freshStart.
   * @throws Exception
   */
  @Test
  public void testFetchEmptyPayload() throws Exception {
    MockServer server = new MockServer(200, TestMetaGlobal.TEST_META_GLOBAL_EMPTY_PAYLOAD_RESPONSE);
    doSession(server);

    assertTrue(calledFreshStart);
  }

  /**
   * No payload means no syncID or storageVersion and therefore we should call freshStart.
   * @throws Exception
   */
  @Test
  public void testFetchNoPayload() throws Exception {
    MockServer server = new MockServer(200, TestMetaGlobal.TEST_META_GLOBAL_NO_PAYLOAD_RESPONSE);
    doSession(server);

    assertTrue(calledFreshStart);
  }

  /**
   * Malformed payload is a server response issue, not a meta/global record
   * issue. This should error out of the sync.
   * @throws Exception
   */
  @Test
  public void testFetchMalformedPayload() throws Exception {
    MockServer server = new MockServer(200, TestMetaGlobal.TEST_META_GLOBAL_MALFORMED_PAYLOAD_RESPONSE);
    doSession(server);

    assertEquals(true, callback.calledError);
    assertEquals(ParseException.class, callback.calledErrorException.getClass());
  }

  protected void doFreshStart(MockServer server) {
    data.startHTTPServer(server);
    WaitHelper.getTestWaiter().performWait(WaitHelper.onThreadRunnable(new Runnable() {
      @Override
      public void run() {
        session.freshStart();
      }
    }));
    data.stopHTTPServer();
  }

  @Test
  public void testFreshStart() throws SyncConfigurationException, IllegalArgumentException, NonObjectJSONException, IOException, ParseException, CryptoException {
    final AtomicBoolean mgUploaded = new AtomicBoolean(false);
    final AtomicBoolean mgDownloaded = new AtomicBoolean(false);
    final MetaGlobal uploadedMg = new MetaGlobal(null, null);

    MockServer server = new MockServer() {
      @Override
      public void handle(Request request, Response response) {
        if (request.getMethod().equals("PUT")) {
          try {
            ExtendedJSONObject body = ExtendedJSONObject.parseJSONObject(request.getContent());
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
        if (mgUploaded.get()) {
          // We shouldn't be trying to download anything after uploading meta/global.
          mgDownloaded.set(true);
        }
        this.handle(request, response, 404, "missing");
      }
    };
    doFreshStart(server);

    assertTrue(this.calledFreshStart);
    assertTrue(this.calledWipeServer);
    assertTrue(this.calledUploadKeys);
    assertTrue(mgUploaded.get());
    assertFalse(mgDownloaded.get());
    assertEquals(GlobalSession.STORAGE_VERSION, uploadedMg.getStorageVersion().longValue());
  }
}
