/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync;

import java.io.IOException;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;
import org.mozilla.gecko.background.testhelpers.BaseMockServerSyncStage;
import org.mozilla.gecko.background.testhelpers.DefaultGlobalSessionCallback;
import org.mozilla.gecko.background.testhelpers.MockRecord;
import org.mozilla.gecko.background.testhelpers.MockSharedPreferences;
import org.mozilla.gecko.background.testhelpers.WBORepository;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.EngineSettings;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.MetaGlobalException;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.SyncConfigurationException;
import org.mozilla.gecko.sync.SynchronizerConfiguration;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.delegates.GlobalSessionCallback;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BasicAuthHeaderProvider;
import org.mozilla.gecko.sync.repositories.domain.Record;
import org.mozilla.gecko.sync.stage.NoSuchStageException;
import org.mozilla.gecko.sync.synchronizer.Synchronizer;

import android.content.SharedPreferences;

/**
 * Test the on-device side effects of reset operations on a stage.
 *
 * See also "TestResetCommands" in the unit test suite.
 */
public class TestResetting extends AndroidSyncTestCase {
  private static final String TEST_USERNAME    = "johndoe";
  private static final String TEST_PASSWORD    = "password";
  private static final String TEST_SYNC_KEY    = "abcdeabcdeabcdeabcdeabcdea";

  @Override
  public void setUp() {
    assertTrue(WaitHelper.getTestWaiter().isIdle());
  }

  /**
   * Set up a mock stage that synchronizes two mock repositories. Apply various
   * reset/sync/wipe permutations and check state.
   */
  public void testResetAndWipeStage() throws Exception {

    final long startTime = System.currentTimeMillis();
    final GlobalSessionCallback callback = createGlobalSessionCallback();
    final GlobalSession session = createDefaultGlobalSession(callback);

    final ExecutableMockServerSyncStage stage = new ExecutableMockServerSyncStage() {
      @Override
      public void onSynchronized(Synchronizer synchronizer) {
        try {
          assertTrue(startTime <= synchronizer.bundleA.getTimestamp());
          assertTrue(startTime <= synchronizer.bundleB.getTimestamp());

          // Call up to allow the usual persistence etc. to happen.
          super.onSynchronized(synchronizer);
        } catch (Throwable e) {
          performNotify(e);
          return;
        }
        performNotify();
      }
    };

    final boolean bumpTimestamps = true;
    WBORepository local = new WBORepository(bumpTimestamps);
    WBORepository remote = new WBORepository(bumpTimestamps);

    stage.name       = "mock";
    stage.collection = "mock";
    stage.local      = local;
    stage.remote     = remote;

    stage.executeSynchronously(session);

    // Verify the persisted values.
    assertConfigTimestampsGreaterThan(stage.leakConfig(), startTime, startTime);

    // Reset.
    stage.resetLocal(session);

    // Verify that they're gone.
    assertConfigTimestampsEqual(stage.leakConfig(), 0, 0);

    // Now sync data, ensure that timestamps come back.
    final long afterReset = System.currentTimeMillis();
    final String recordGUID = "abcdefghijkl";
    local.wbos.put(recordGUID, new MockRecord(recordGUID, "mock", startTime, false));

    // Sync again with data and verify timestamps and data.
    stage.executeSynchronously(session);

    assertConfigTimestampsGreaterThan(stage.leakConfig(), afterReset, afterReset);
    assertEquals(1, remote.wbos.size());
    assertEquals(1, local.wbos.size());

    Record remoteRecord = remote.wbos.get(recordGUID);
    assertNotNull(remoteRecord);
    assertNotNull(local.wbos.get(recordGUID));
    assertEquals(recordGUID, remoteRecord.guid);
    assertTrue(afterReset <= remoteRecord.lastModified);

    // Reset doesn't clear data.
    stage.resetLocal(session);
    assertConfigTimestampsEqual(stage.leakConfig(), 0, 0);
    assertEquals(1, remote.wbos.size());
    assertEquals(1, local.wbos.size());
    remoteRecord = remote.wbos.get(recordGUID);
    assertNotNull(remoteRecord);
    assertNotNull(local.wbos.get(recordGUID));

    // Wipe does. Recover from reset...
    final long beforeWipe = System.currentTimeMillis();
    stage.executeSynchronously(session);
    assertEquals(1, remote.wbos.size());
    assertEquals(1, local.wbos.size());
    assertConfigTimestampsGreaterThan(stage.leakConfig(), beforeWipe, beforeWipe);

    // ... then wipe.
    stage.wipeLocal(session);
    assertConfigTimestampsEqual(stage.leakConfig(), 0, 0);
    assertEquals(1, remote.wbos.size());     // We don't wipe the server.
    assertEquals(0, local.wbos.size());      // We do wipe local.
  }

  /**
   * A stage that joins two Repositories with no wrapping.
   */
  public class ExecutableMockServerSyncStage extends BaseMockServerSyncStage {
    /**
     * Run this stage synchronously.
     */
    public void executeSynchronously(final GlobalSession session) {
      final BaseMockServerSyncStage self = this;
      performWait(new Runnable() {
        @Override
        public void run() {
          try {
            self.execute(session);
          } catch (NoSuchStageException e) {
            performNotify(e);
          }
        }
      });
    }
  }

  private GlobalSession createDefaultGlobalSession(final GlobalSessionCallback callback) throws SyncConfigurationException, IllegalArgumentException, NonObjectJSONException, IOException, ParseException, CryptoException {

    final KeyBundle keyBundle = new KeyBundle(TEST_USERNAME, TEST_SYNC_KEY);
    final AuthHeaderProvider authHeaderProvider = new BasicAuthHeaderProvider(TEST_USERNAME, TEST_PASSWORD);
    final SharedPreferences prefs = new MockSharedPreferences();
    final SyncConfiguration config = new SyncConfiguration(TEST_USERNAME, authHeaderProvider, prefs);
    config.syncKeyBundle = keyBundle;
    return new GlobalSession(config, callback, getApplicationContext(), null) {
      @Override
      public boolean isEngineRemotelyEnabled(String engineName,
                                     EngineSettings engineSettings)
        throws MetaGlobalException {
        return true;
      }

      @Override
      public void advance() {
        // So we don't proceed and run other stages.
      }
    };
  }

  private static GlobalSessionCallback createGlobalSessionCallback() {
    return new DefaultGlobalSessionCallback() {

      @Override
      public void handleAborted(GlobalSession globalSession, String reason) {
        performNotify(new Exception("Aborted"));
      }

      @Override
      public void handleError(GlobalSession globalSession, Exception ex) {
        performNotify(ex);
      }
    };
  }

  private static void assertConfigTimestampsGreaterThan(SynchronizerConfiguration config, long local, long remote) {
    assertTrue(local <= config.localBundle.getTimestamp());
    assertTrue(remote <= config.remoteBundle.getTimestamp());
  }

  private static void assertConfigTimestampsEqual(SynchronizerConfiguration config, long local, long remote) {
    assertEquals(local, config.localBundle.getTimestamp());
    assertEquals(remote, config.remoteBundle.getTimestamp());
  }
}
