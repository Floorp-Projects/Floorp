/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.util.HashMap;

import org.json.simple.parser.ParseException;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.DefaultGlobalSessionCallback;
import org.mozilla.gecko.background.testhelpers.MockPrefsGlobalSession;
import org.mozilla.gecko.background.testhelpers.MockServerSyncStage;
import org.mozilla.gecko.background.testhelpers.MockSharedPreferences;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.CommandProcessor;
import org.mozilla.gecko.sync.EngineSettings;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.MetaGlobalException;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.SyncConfigurationException;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.delegates.GlobalSessionCallback;
import org.mozilla.gecko.sync.net.BasicAuthHeaderProvider;
import org.mozilla.gecko.sync.stage.GlobalSyncStage;
import org.mozilla.gecko.sync.stage.GlobalSyncStage.Stage;

import android.content.SharedPreferences;
import org.robolectric.RobolectricGradleTestRunner;

/**
 * Test that reset commands properly invoke the reset methods on the correct stage.
 */
@RunWith(RobolectricGradleTestRunner.class)
public class TestResetCommands {
  private static final String TEST_USERNAME    = "johndoe";
  private static final String TEST_PASSWORD    = "password";
  private static final String TEST_SYNC_KEY    = "abcdeabcdeabcdeabcdeabcdea";

  public static void performNotify() {
    WaitHelper.getTestWaiter().performNotify();
  }

  public static void performNotify(Throwable e) {
    WaitHelper.getTestWaiter().performNotify(e);
  }

  public static void performWait(Runnable runnable) {
    WaitHelper.getTestWaiter().performWait(runnable);
  }

  @Before
  public void setUp() {
    assertTrue(WaitHelper.getTestWaiter().isIdle());
  }

  @Test
  public void testHandleResetCommand() throws SyncConfigurationException, IllegalArgumentException, NonObjectJSONException, IOException, ParseException, CryptoException {
    // Create a global session.
    // Set up stage mappings for a real stage name (because they're looked up by name
    // in an enumeration) pointing to our fake stage.
    // Send a reset command.
    // Verify that reset is called on our stage.

    class Result {
      public boolean called = false;
    }

    final Result yes = new Result();
    final Result no  = new Result();
    final GlobalSessionCallback callback = createGlobalSessionCallback();

    // So we can poke at stages separately.
    final HashMap<Stage, GlobalSyncStage> stagesToRun = new HashMap<Stage, GlobalSyncStage>();

    // Side-effect: modifies global command processor.
    final SharedPreferences prefs = new MockSharedPreferences();
    final SyncConfiguration config = new SyncConfiguration(TEST_USERNAME, new BasicAuthHeaderProvider(TEST_USERNAME, TEST_PASSWORD), prefs);
    config.syncKeyBundle = new KeyBundle(TEST_USERNAME, TEST_SYNC_KEY);
    final GlobalSession session = new MockPrefsGlobalSession(config, callback, null, null) {
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

      @Override
      public void prepareStages() {
        this.stages = stagesToRun;
      }
    };

    final MockServerSyncStage stageGetsReset = new MockServerSyncStage() {
      @Override
      public void resetLocal() {
        yes.called = true;
      }
    };

    final MockServerSyncStage stageNotReset = new MockServerSyncStage() {
      @Override
      public void resetLocal() {
        no.called = true;
      }
    };

    stagesToRun.put(Stage.syncBookmarks, stageGetsReset);
    stagesToRun.put(Stage.syncHistory,   stageNotReset);

    final String resetBookmarks = "{\"args\":[\"bookmarks\"],\"command\":\"resetEngine\"}";
    ExtendedJSONObject unparsedCommand = new ExtendedJSONObject(resetBookmarks);
    CommandProcessor processor = CommandProcessor.getProcessor();
    processor.processCommand(session, unparsedCommand);

    assertTrue(yes.called);
    assertFalse(no.called);
  }

  public void testHandleWipeCommand() {
    // TODO
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

      @Override
      public void handleSuccess(GlobalSession globalSession) {
      }
    };
  }
}
