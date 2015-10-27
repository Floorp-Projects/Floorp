/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport.prune;

import java.io.File;

import org.mozilla.gecko.background.healthreport.HealthReportDatabaseStorage;
import org.mozilla.gecko.background.helpers.FakeProfileTestCase;

import android.content.Context;

public class TestPrunePolicyDatabaseStorage extends FakeProfileTestCase {
  public static class MockPrunePolicyDatabaseStorage extends PrunePolicyDatabaseStorage {
    public final MockHealthReportDatabaseStorage storage;

    public int currentEnvironmentID;

    public MockPrunePolicyDatabaseStorage(final Context context, final String profilePath) {
      super(context, profilePath);
      storage = new MockHealthReportDatabaseStorage(context, new File(profilePath));

      currentEnvironmentID = -1;
    }

    @Override
    public HealthReportDatabaseStorage getStorage() {
      return storage;
    }

    @Override
    public int getCurrentEnvironmentID() {
      return currentEnvironmentID;
    }
  }

  public static class MockHealthReportDatabaseStorage extends HealthReportDatabaseStorage {
    private boolean wasPruneEventsCalled = false;
    private boolean wasPruneEnvironmentsCalled = false;
    private boolean wasDeleteDataBeforeCalled = false;
    private boolean wasDisableAutoVacuumingCalled = false;
    private boolean wasVacuumCalled = false;

    public MockHealthReportDatabaseStorage(final Context context, final File file) {
      super(context, file);
    }

    // We use spies here to avoid doing expensive DB operations (which are tested elsewhere).
    @Override
    public void pruneEvents(final int count) {
      wasPruneEventsCalled = true;
    }

    @Override
    public void pruneEnvironments(final int count) {
      wasPruneEnvironmentsCalled = true;
    }

    @Override
    public int deleteDataBefore(final long time, final int curEnv) {
      wasDeleteDataBeforeCalled = true;
      return -1;
    }

    @Override
    public void disableAutoVacuuming() {
      wasDisableAutoVacuumingCalled = true;
    }

    @Override
    public void vacuum() {
      wasVacuumCalled = true;
    }
  }

  public MockPrunePolicyDatabaseStorage policyStorage;

  @Override
  public void setUp() throws Exception {
    super.setUp();
    policyStorage = new MockPrunePolicyDatabaseStorage(context, "profilePath");
  }

  @Override
  public void tearDown() throws Exception {
    super.tearDown();
  }

  public void testPruneEvents() throws Exception {
    policyStorage.pruneEvents(0);
    assertTrue(policyStorage.storage.wasPruneEventsCalled);
  }

  public void testPruneEnvironments() throws Exception {
    policyStorage.pruneEnvironments(0);
    assertTrue(policyStorage.storage.wasPruneEnvironmentsCalled);
  }

  public void testDeleteDataBefore() throws Exception {
    policyStorage.deleteDataBefore(-1);
    assertTrue(policyStorage.storage.wasDeleteDataBeforeCalled);
  }

  public void testCleanup() throws Exception {
    policyStorage.cleanup();
    assertTrue(policyStorage.storage.wasDisableAutoVacuumingCalled);
    assertTrue(policyStorage.storage.wasVacuumCalled);
  }
}
