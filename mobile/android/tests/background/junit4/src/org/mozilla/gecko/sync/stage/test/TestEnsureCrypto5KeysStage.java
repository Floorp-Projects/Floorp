/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.stage.test;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.android.sync.test.helpers.HTTPServerTestHelper;
import org.mozilla.android.sync.test.helpers.MockGlobalSessionCallback;
import org.mozilla.android.sync.test.helpers.MockServer;
import org.mozilla.gecko.background.testhelpers.MockGlobalSession;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.AlreadySyncingException;
import org.mozilla.gecko.sync.CollectionKeys;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.InfoCollections;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.stage.EnsureCrypto5KeysStage;
import org.mozilla.gecko.sync.stage.GlobalSyncStage.Stage;
import org.simpleframework.http.Request;
import org.simpleframework.http.Response;

import java.net.URI;
import java.util.ArrayList;
import java.util.Collection;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

@RunWith(TestRunner.class)
public class TestEnsureCrypto5KeysStage {
  private int          TEST_PORT                = HTTPServerTestHelper.getTestPort();
  private final String TEST_CLUSTER_URL         = "http://localhost:" + TEST_PORT;
  private final String TEST_USERNAME            = "johndoe";
  private final String TEST_PASSWORD            = "password";
  private final String TEST_SYNC_KEY            = "abcdeabcdeabcdeabcdeabcdea";

  private final String TEST_JSON_NO_CRYPTO =
      "{\"history\":1.3319567131E9}";
  private final String TEST_JSON_OLD_CRYPTO =
      "{\"history\":1.3319567131E9,\"crypto\":1.1E9}";
  private final String TEST_JSON_NEW_CRYPTO =
      "{\"history\":1.3319567131E9,\"crypto\":3.1E9}";

  private HTTPServerTestHelper data = new HTTPServerTestHelper();

  private KeyBundle syncKeyBundle;
  private MockGlobalSessionCallback callback;
  private GlobalSession session;

  private boolean calledResetStages;
  private Collection<String> stagesReset;

  @Before
  public void setUp() throws Exception {
    syncKeyBundle = new KeyBundle(TEST_USERNAME, TEST_SYNC_KEY);
    callback = new MockGlobalSessionCallback();
    session = new MockGlobalSession(TEST_USERNAME, TEST_PASSWORD,
      syncKeyBundle, callback) {
      @Override
      protected void prepareStages() {
        super.prepareStages();
        withStage(Stage.ensureKeysStage, new EnsureCrypto5KeysStage());
      }

      @Override
      public void resetStagesByEnum(Collection<Stage> stages) {
        calledResetStages = true;
        stagesReset = new ArrayList<String>();
        for (Stage stage : stages) {
          stagesReset.add(stage.name());
        }
      }

      @Override
      public void resetStagesByName(Collection<String> names) {
        calledResetStages = true;
        stagesReset = names;
      }
    };
    session.config.setClusterURL(new URI(TEST_CLUSTER_URL));

    // Set info collections to not have crypto.
    final ExtendedJSONObject noCrypto = new ExtendedJSONObject(TEST_JSON_NO_CRYPTO);
    session.config.infoCollections = new InfoCollections(noCrypto);
    calledResetStages = false;
    stagesReset = null;
  }

  public void doSession(MockServer server) {
    data.startHTTPServer(server);
    try {
      WaitHelper.getTestWaiter().performWait(new Runnable() {
        @Override
        public void run() {
          try {
            session.start();
          } catch (AlreadySyncingException e) {
            WaitHelper.getTestWaiter().performNotify(e);
          }
        }
      });
    } finally {
    data.stopHTTPServer();
    }
  }

  @Test
  public void testDownloadUsesPersisted() throws Exception {
    session.config.infoCollections = new InfoCollections(new ExtendedJSONObject
            (TEST_JSON_OLD_CRYPTO));
    session.config.persistedCryptoKeys().persistLastModified(System.currentTimeMillis());

    assertNull(session.config.collectionKeys);
    final CollectionKeys keys = CollectionKeys.generateCollectionKeys();
    keys.setDefaultKeyBundle(syncKeyBundle);
    session.config.persistedCryptoKeys().persistKeys(keys);

    MockServer server = new MockServer() {
      public void handle(Request request, Response response) {
        this.handle(request, response, 404, "should not be called!");
      }
    };

    doSession(server);

    assertTrue(callback.calledSuccess);
    assertNotNull(session.config.collectionKeys);
    assertTrue(CollectionKeys.differences(session.config.collectionKeys, keys).isEmpty());
  }

  @Test
  public void testDownloadFetchesNew() throws Exception {
    session.config.infoCollections = new InfoCollections(new ExtendedJSONObject(TEST_JSON_NEW_CRYPTO));
    session.config.persistedCryptoKeys().persistLastModified(System.currentTimeMillis());

    assertNull(session.config.collectionKeys);
    final CollectionKeys keys = CollectionKeys.generateCollectionKeys();
    keys.setDefaultKeyBundle(syncKeyBundle);
    session.config.persistedCryptoKeys().persistKeys(keys);

    MockServer server = new MockServer() {
      public void handle(Request request, Response response) {
        try {
          CryptoRecord rec = keys.asCryptoRecord();
          rec.keyBundle = syncKeyBundle;
          rec.encrypt();
          this.handle(request, response, 200, rec.toJSONString());
        } catch (Exception e) {
          throw new RuntimeException(e);
        }
      }
    };

    doSession(server);

    assertTrue(callback.calledSuccess);
    assertNotNull(session.config.collectionKeys);
    assertTrue(session.config.collectionKeys.equals(keys));
  }

  /**
   * Change the default key but keep one collection key the same. Should reset
   * all but that one collection.
   */
  @Test
  public void testDownloadResetsOnDifferentDefaultKey() throws Exception {
    String TEST_COLLECTION = "bookmarks";

    session.config.infoCollections = new InfoCollections(new ExtendedJSONObject(TEST_JSON_NEW_CRYPTO));
    session.config.persistedCryptoKeys().persistLastModified(System.currentTimeMillis());

    KeyBundle keyBundle = KeyBundle.withRandomKeys();
    assertNull(session.config.collectionKeys);
    final CollectionKeys keys = CollectionKeys.generateCollectionKeys();
    keys.setKeyBundleForCollection(TEST_COLLECTION, keyBundle);
    session.config.persistedCryptoKeys().persistKeys(keys);
    keys.setDefaultKeyBundle(syncKeyBundle); // Change the default key bundle, but keep "bookmarks" the same.

    MockServer server = new MockServer() {
      public void handle(Request request, Response response) {
        try {
          CryptoRecord rec = keys.asCryptoRecord();
          rec.keyBundle = syncKeyBundle;
          rec.encrypt();
          this.handle(request, response, 200, rec.toJSONString());
        } catch (Exception e) {
          throw new RuntimeException(e);
        }
      }
    };

    doSession(server);

    assertTrue(calledResetStages);
    Collection<String> allButCollection = new ArrayList<String>();
    for (Stage stage : Stage.getNamedStages()) {
      allButCollection.add(stage.getRepositoryName());
    }
    allButCollection.remove(TEST_COLLECTION);
    assertTrue(stagesReset.containsAll(allButCollection));
    assertTrue(allButCollection.containsAll(stagesReset));
    assertTrue(callback.calledError);
  }

  @Test
  public void testDownloadResetsEngineOnDifferentKey() throws Exception {
    final String TEST_COLLECTION = "history";

    session.config.infoCollections = new InfoCollections(new ExtendedJSONObject(TEST_JSON_NEW_CRYPTO));
    session.config.persistedCryptoKeys().persistLastModified(System.currentTimeMillis());

    assertNull(session.config.collectionKeys);
    final CollectionKeys keys = CollectionKeys.generateCollectionKeys();
    session.config.persistedCryptoKeys().persistKeys(keys);
    keys.setKeyBundleForCollection(TEST_COLLECTION, syncKeyBundle); // Change one key bundle.

    CryptoRecord rec = keys.asCryptoRecord();
    rec.keyBundle = syncKeyBundle;
    rec.encrypt();
    MockServer server = new MockServer(200, rec.toJSONString());

    doSession(server);

    assertTrue(calledResetStages);
    assertNotNull(stagesReset);
    assertEquals(1, stagesReset.size());
    assertTrue(stagesReset.contains(TEST_COLLECTION));
    assertTrue(callback.calledError);
  }
}
