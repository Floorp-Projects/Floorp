/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport.prune.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.mozilla.gecko.background.healthreport.HealthReportConstants;
import org.mozilla.gecko.background.healthreport.prune.PrunePolicy;
import org.mozilla.gecko.background.healthreport.prune.PrunePolicyStorage;
import org.mozilla.gecko.background.testhelpers.MockSharedPreferences;

import android.content.SharedPreferences;

public class TestPrunePolicy {
  public static class MockPrunePolicy extends PrunePolicy {
    public MockPrunePolicy(final PrunePolicyStorage storage, final SharedPreferences sharedPrefs) {
      super(storage, sharedPrefs);
    }

    @Override
    public boolean attemptPruneBySize(final long time) {
      return super.attemptPruneBySize(time);
    }

    @Override
    public boolean attemptExpiration(final long time) {
      return super.attemptExpiration(time);
    }

    @Override
    protected boolean attemptStorageCleanup(final long time) {
      return super.attemptStorageCleanup(time);
    }
  }

  public static class MockPrunePolicyStorage implements PrunePolicyStorage {
    public int eventCount = -1;
    public int environmentCount = -1;

    // TODO: Spies - should we be using a framework?
    // TODO: Each method was called with what args?
    public boolean wasPruneEventsCalled = false;
    public boolean wasPruneEnvironmentsCalled = false;
    public boolean wasDeleteDataBeforeCalled = false;
    public boolean wasCleanupCalled = false;

    public MockPrunePolicyStorage() { }

    public void pruneEvents(final int maxNumToPrune) {
      wasPruneEventsCalled = true;
    }

    public void pruneEnvironments(final int numToPrune) {
      wasPruneEnvironmentsCalled = true;
    }

    public int deleteDataBefore(final long time) {
      wasDeleteDataBeforeCalled = true;
      return -1;
    }

    public void cleanup() {
      wasCleanupCalled = true;
    }

    public int getEventCount() { return eventCount; }
    public int getEnvironmentCount() { return environmentCount; }

    public void close() { /* Nothing to cleanup. */ }
  }

  // An arbitrary value so that each test doesn't need to specify its own time.
  public static final long START_TIME = 1000;

  public MockPrunePolicy policy;
  public MockPrunePolicyStorage storage;
  public SharedPreferences sharedPrefs;

  @Before
  public void setUp() throws Exception {
    sharedPrefs = new MockSharedPreferences();
    storage = new MockPrunePolicyStorage();
    policy = new MockPrunePolicy(storage, sharedPrefs);
  }

  public boolean attemptPruneBySize(final long time) {
    final boolean retval = policy.attemptPruneBySize(time);
    // This commit may be deferred over multiple methods so we ensure it runs.
    sharedPrefs.edit().commit();
    return retval;
  }

  public boolean attemptExpiration(final long time) {
    final boolean retval = policy.attemptExpiration(time);
    // This commit may be deferred over multiple methods so we ensure it runs.
    sharedPrefs.edit().commit();
    return retval;
  }

  public boolean attemptStorageCleanup(final long time) {
    final boolean retval = policy.attemptStorageCleanup(time);
    // This commit may be deferred over multiple methods so we ensure it runs.
    sharedPrefs.edit().commit();
    return retval;
  }

  @Test
  public void testAttemptPruneBySizeInit() throws Exception {
    assertFalse(containsNextPruneBySizeTime());
    attemptPruneBySize(START_TIME);

    // Next time should be initialized.
    assertTrue(containsNextPruneBySizeTime());
    assertTrue(getNextPruneBySizeTime() > 0);
    assertFalse(storage.wasPruneEventsCalled);
    assertFalse(storage.wasPruneEnvironmentsCalled);
  }

  @Test
  public void testAttemptPruneBySizeEarly() throws Exception {
    final long nextTime = START_TIME + 1;
    setNextPruneBySizeTime(nextTime);
    attemptPruneBySize(START_TIME);

    // We didn't prune so next time remains the same.
    assertEquals(nextTime, getNextPruneBySizeTime());
    assertFalse(storage.wasPruneEventsCalled);
    assertFalse(storage.wasPruneEnvironmentsCalled);
  }

  @Test
  public void testAttemptPruneBySizeSkewed() throws Exception {
    setNextPruneBySizeTime(START_TIME + getMinimumTimeBetweenPruneBySizeChecks() + 1);
    attemptPruneBySize(START_TIME);

    // Skewed so the next time is reset.
    assertEquals(START_TIME + getMinimumTimeBetweenPruneBySizeChecks(), getNextPruneBySizeTime());
    assertFalse(storage.wasPruneEventsCalled);
    assertFalse(storage.wasPruneEnvironmentsCalled);
  }

  @Test
  public void testAttemptPruneBySizeTooFewEnvironments() throws Exception {
    setNextPruneBySizeTime(START_TIME - 1);
    storage.environmentCount = getMaximumEnvironmentCount(); // Too few to prune.
    attemptPruneBySize(START_TIME);

    assertEquals(START_TIME + getMinimumTimeBetweenPruneBySizeChecks(), getNextPruneBySizeTime());
    assertFalse(storage.wasPruneEnvironmentsCalled);
  }

  @Test
  public void testAttemptPruneBySizeEnvironmentsSuccess() throws Exception {
    setNextPruneBySizeTime(START_TIME - 1);
    storage.environmentCount = getMaximumEnvironmentCount() + 1;
    attemptPruneBySize(START_TIME);

    assertEquals(START_TIME + getMinimumTimeBetweenPruneBySizeChecks(), getNextPruneBySizeTime());
    assertTrue(storage.wasPruneEnvironmentsCalled);
  }

  @Test
  public void testAttemptPruneBySizeTooFewEvents() throws Exception {
    setNextPruneBySizeTime(START_TIME - 1);
    storage.eventCount = getMaximumEventCount(); // Too few to prune.
    attemptPruneBySize(START_TIME);

    assertEquals(START_TIME + getMinimumTimeBetweenPruneBySizeChecks(), getNextPruneBySizeTime());
    assertFalse(storage.wasPruneEventsCalled);
  }

  @Test
  public void testAttemptPruneBySizeEventsSuccess() throws Exception {
    setNextPruneBySizeTime(START_TIME - 1);
    storage.eventCount = getMaximumEventCount() + 1;
    attemptPruneBySize(START_TIME);

    assertEquals(START_TIME + getMinimumTimeBetweenPruneBySizeChecks(), getNextPruneBySizeTime());
    assertTrue(storage.wasPruneEventsCalled);
  }

  @Test
  public void testAttemptExpirationInit() throws Exception {
    assertFalse(containsNextExpirationTime());
    attemptExpiration(START_TIME);

    // Next time should be initialized.
    assertTrue(containsNextExpirationTime());
    assertTrue(getNextExpirationTime() > 0);
    assertFalse(storage.wasDeleteDataBeforeCalled);
  }

  @Test
  public void testAttemptExpirationEarly() throws Exception {
    final long nextTime = START_TIME + 1;
    setNextExpirationTime(nextTime);
    attemptExpiration(START_TIME);

    // We didn't prune so next time remains the same.
    assertEquals(nextTime, getNextExpirationTime());
    assertFalse(storage.wasDeleteDataBeforeCalled);
  }

  @Test
  public void testAttemptExpirationSkewed() throws Exception {
    setNextExpirationTime(START_TIME + getMinimumTimeBetweenExpirationChecks() + 1);
    attemptExpiration(START_TIME);

    // Skewed so the next time is reset.
    assertEquals(START_TIME + getMinimumTimeBetweenExpirationChecks(), getNextExpirationTime());
    assertFalse(storage.wasDeleteDataBeforeCalled);
  }

  @Test
  public void testAttemptExpirationSuccess() throws Exception {
    setNextExpirationTime(START_TIME - 1);
    attemptExpiration(START_TIME);

    assertEquals(START_TIME + getMinimumTimeBetweenExpirationChecks(), getNextExpirationTime());
    assertTrue(storage.wasDeleteDataBeforeCalled);
  }

  @Test
  public void testAttemptCleanupInit() throws Exception {
    assertFalse(containsNextCleanupTime());
    attemptStorageCleanup(START_TIME);

    // Next time should be initialized.
    assertTrue(containsNextCleanupTime());
    assertTrue(getNextCleanupTime() > 0);
    assertFalse(storage.wasCleanupCalled);
  }

  @Test
  public void testAttemptCleanupEarly() throws Exception {
    final long nextTime = START_TIME + 1;
    setNextCleanupTime(nextTime);
    attemptStorageCleanup(START_TIME);

    // We didn't prune so next time remains the same.
    assertEquals(nextTime, getNextCleanupTime());
    assertFalse(storage.wasCleanupCalled);
  }

  @Test
  public void testAttemptCleanupSkewed() throws Exception {
    setNextCleanupTime(START_TIME + getMinimumTimeBetweenCleanupChecks() + 1);
    attemptStorageCleanup(START_TIME);

    // Skewed so the next time is reset.
    assertEquals(START_TIME + getMinimumTimeBetweenCleanupChecks(), getNextCleanupTime());
    assertFalse(storage.wasCleanupCalled);
  }

  @Test
  public void testAttemptCleanupSuccess() throws Exception {
    setNextCleanupTime(START_TIME - 1);
    attemptStorageCleanup(START_TIME);

    assertEquals(START_TIME + getMinimumTimeBetweenCleanupChecks(), getNextCleanupTime());
    assertTrue(storage.wasCleanupCalled);
  }

  public int getMaximumEnvironmentCount() {
    return HealthReportConstants.MAX_ENVIRONMENT_COUNT;
  }

  public int getMaximumEventCount() {
    return HealthReportConstants.MAX_EVENT_COUNT;
  }

  public long getMinimumTimeBetweenPruneBySizeChecks() {
    return HealthReportConstants.MINIMUM_TIME_BETWEEN_PRUNE_BY_SIZE_CHECKS_MILLIS;
  }

  public long getNextPruneBySizeTime() {
    return sharedPrefs.getLong(HealthReportConstants.PREF_PRUNE_BY_SIZE_TIME, -1);
  }

  public void setNextPruneBySizeTime(final long time) {
    sharedPrefs.edit().putLong(HealthReportConstants.PREF_PRUNE_BY_SIZE_TIME, time).commit();
  }

  public boolean containsNextPruneBySizeTime() {
    return sharedPrefs.contains(HealthReportConstants.PREF_PRUNE_BY_SIZE_TIME);
  }

  public long getMinimumTimeBetweenExpirationChecks() {
    return HealthReportConstants.MINIMUM_TIME_BETWEEN_EXPIRATION_CHECKS_MILLIS;
  }

  public long getNextExpirationTime() {
    return sharedPrefs.getLong(HealthReportConstants.PREF_EXPIRATION_TIME, -1);
  }

  public void setNextExpirationTime(final long time) {
    sharedPrefs.edit().putLong(HealthReportConstants.PREF_EXPIRATION_TIME, time).commit();
  }

  public boolean containsNextExpirationTime() {
    return sharedPrefs.contains(HealthReportConstants.PREF_EXPIRATION_TIME);
  }

  public long getMinimumTimeBetweenCleanupChecks() {
    return HealthReportConstants.MINIMUM_TIME_BETWEEN_CLEANUP_CHECKS_MILLIS;
  }

  public long getNextCleanupTime() {
    return sharedPrefs.getLong(HealthReportConstants.PREF_CLEANUP_TIME, -1);
  }

  public void setNextCleanupTime(final long time) {
    sharedPrefs.edit().putLong(HealthReportConstants.PREF_CLEANUP_TIME, time).commit();
  }

  public boolean containsNextCleanupTime() {
    return sharedPrefs.contains(HealthReportConstants.PREF_CLEANUP_TIME);
  }
}
