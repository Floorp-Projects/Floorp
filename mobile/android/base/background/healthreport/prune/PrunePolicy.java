/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport.prune;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.healthreport.HealthReportConstants;

import android.content.SharedPreferences;

/**
 * Manages scheduling of the pruning of old Firefox Health Report data.
 *
 * There are three main actions that take place:
 *   1) Excessive storage pruning: The recorded data is taking up an unreasonable amount of space.
 *   2) Expired data pruning: Data that is kept around longer than is useful.
 *   3) Cleanup: To deal with storage maintenance (e.g. bloat and fragmentation)
 *
 * (1) and (2) are performed periodically on their own schedules. (3) will activate after a
 * certain duration but only after (1) or (2) is performed.
 */
public class PrunePolicy {
  public static final String LOG_TAG = PrunePolicy.class.getSimpleName();

  protected final PrunePolicyStorage storage;
  protected final SharedPreferences sharedPreferences;
  protected final Editor editor;

  public PrunePolicy(final PrunePolicyStorage storage, final SharedPreferences sharedPrefs) {
    this.storage = storage;
    this.sharedPreferences = sharedPrefs;
    this.editor = new Editor(this.sharedPreferences.edit());
  }

  protected SharedPreferences getSharedPreferences() {
    return this.sharedPreferences;
  }

  public void tick(final long time) {
    try {
      try {
        boolean pruned = attemptPruneBySize(time);
        pruned = attemptExpiration(time) || pruned;
        // We only need to cleanup after a large pruning.
        if (pruned) {
          attemptStorageCleanup(time);
        }
      } catch (Exception e) {
        // While catching Exception is ordinarily bad form, this Service runs in the same process
        // as Fennec so if we crash, it crashes. Additionally, this Service runs regularly so
        // these crashes could be regular. Thus, we choose to quietly fail instead.
        Logger.error(LOG_TAG, "Got exception pruning document.", e);
      } finally {
        editor.commit();
      }
    } catch (Exception e) {
      Logger.error(LOG_TAG, "Got exception committing to SharedPreferences.", e);
    } finally {
      storage.close();
    }
  }

  protected boolean attemptPruneBySize(final long time) {
    final long nextPrune = getNextPruneBySizeTime();
    if (nextPrune < 0) {
      Logger.debug(LOG_TAG, "Initializing prune-by-size time.");
      editor.setNextPruneBySizeTime(time + getMinimumTimeBetweenPruneBySizeChecks());
      return false;
    }

    // If the system clock is skewed into the past, making the time between prunes too long, reset
    // the clock.
    if (nextPrune > getMinimumTimeBetweenPruneBySizeChecks() + time) {
      Logger.debug(LOG_TAG, "Clock skew detected - resetting prune-by-size time.");
      editor.setNextPruneBySizeTime(time + getMinimumTimeBetweenPruneBySizeChecks());
      return false;
    }

    if (nextPrune > time) {
      Logger.debug(LOG_TAG, "Skipping prune-by-size - wait period has not yet elapsed.");
      return false;
    }

    Logger.debug(LOG_TAG, "Attempting prune-by-size.");

    // Prune environments first because their cascading deletions may delete some events. These
    // environments are pruned in order of least-recently used first. Note that orphaned
    // environments are ignored here and should be removed elsewhere.
    final int environmentCount = storage.getEnvironmentCount();
    if (environmentCount > getMaxEnvironmentCount()) {
      final int environmentPruneCount = environmentCount - getEnvironmentCountAfterPrune();
      Logger.debug(LOG_TAG, "Pruning " + environmentPruneCount + " environments.");
      storage.pruneEnvironments(environmentPruneCount);
    }

    final int eventCount = storage.getEventCount();
    if (eventCount > getMaxEventCount()) {
      final int eventPruneCount = eventCount - getEventCountAfterPrune();
      Logger.debug(LOG_TAG, "Pruning up to " + eventPruneCount + " events.");
      storage.pruneEvents(eventPruneCount);
    }
    editor.setNextPruneBySizeTime(time + getMinimumTimeBetweenPruneBySizeChecks());
    return true;
  }

  protected boolean attemptExpiration(final long time) {
    final long nextPrune = getNextExpirationTime();
    if (nextPrune < 0) {
      Logger.debug(LOG_TAG, "Initializing expiration time.");
      editor.setNextExpirationTime(time + getMinimumTimeBetweenExpirationChecks());
      return false;
    }

    // If the system clock is skewed into the past, making the time between prunes too long, reset
    // the clock.
    if (nextPrune > getMinimumTimeBetweenExpirationChecks() + time) {
      Logger.debug(LOG_TAG, "Clock skew detected - resetting expiration time.");
      editor.setNextExpirationTime(time + getMinimumTimeBetweenExpirationChecks());
      return false;
    }

    if (nextPrune > time) {
      Logger.debug(LOG_TAG, "Skipping expiration - wait period has not yet elapsed.");
      return false;
    }

    final long oldEventTime = time - getEventExistenceDuration();
    Logger.debug(LOG_TAG, "Pruning data older than " + oldEventTime + ".");
    storage.deleteDataBefore(oldEventTime);
    editor.setNextExpirationTime(time + getMinimumTimeBetweenExpirationChecks());
    return true;
  }

  protected boolean attemptStorageCleanup(final long time) {
    // Cleanup if max duration since last cleanup is exceeded.
    final long nextCleanup = getNextCleanupTime();
    if (nextCleanup < 0) {
      Logger.debug(LOG_TAG, "Initializing cleanup time.");
      editor.setNextCleanupTime(time + getMinimumTimeBetweenCleanupChecks());
      return false;
    }

    // If the system clock is skewed into the past, making the time between cleanups too long,
    // reset the clock.
    if (nextCleanup > getMinimumTimeBetweenCleanupChecks() + time) {
      Logger.debug(LOG_TAG, "Clock skew detected - resetting cleanup time.");
      editor.setNextCleanupTime(time + getMinimumTimeBetweenCleanupChecks());
      return false;
    }

    if (nextCleanup > time) {
      Logger.debug(LOG_TAG, "Skipping cleanup - wait period has not yet elapsed.");
      return false;
    }

    editor.setNextCleanupTime(time + getMinimumTimeBetweenCleanupChecks());
    Logger.debug(LOG_TAG, "Cleaning up storage.");
    storage.cleanup();
    return true;
  }

  protected static class Editor {
    protected final SharedPreferences.Editor editor;

    public Editor(final SharedPreferences.Editor editor) {
      this.editor = editor;
    }

    public void commit() {
      editor.commit();
    }

    public Editor setNextExpirationTime(final long time) {
      editor.putLong(HealthReportConstants.PREF_EXPIRATION_TIME, time);
      return this;
    }

    public Editor setNextPruneBySizeTime(final long time) {
      editor.putLong(HealthReportConstants.PREF_PRUNE_BY_SIZE_TIME, time);
      return this;
    }

    public Editor setNextCleanupTime(final long time) {
      editor.putLong(HealthReportConstants.PREF_CLEANUP_TIME, time);
      return this;
    }
  }

  private long getNextExpirationTime() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_EXPIRATION_TIME, -1L);
  }

  private long getEventExistenceDuration() {
    return HealthReportConstants.EVENT_EXISTENCE_DURATION;
  }

  private long getMinimumTimeBetweenExpirationChecks() {
    return HealthReportConstants.MINIMUM_TIME_BETWEEN_EXPIRATION_CHECKS_MILLIS;
  }

  private long getNextPruneBySizeTime() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_PRUNE_BY_SIZE_TIME, -1L);
  }

  private long getMinimumTimeBetweenPruneBySizeChecks() {
    return HealthReportConstants.MINIMUM_TIME_BETWEEN_PRUNE_BY_SIZE_CHECKS_MILLIS;
  }

  private int getMaxEnvironmentCount() {
    return HealthReportConstants.MAX_ENVIRONMENT_COUNT;
  }

  private int getEnvironmentCountAfterPrune() {
    return HealthReportConstants.ENVIRONMENT_COUNT_AFTER_PRUNE;
  }

  private int getMaxEventCount() {
    return HealthReportConstants.MAX_EVENT_COUNT;
  }

  private int getEventCountAfterPrune() {
    return HealthReportConstants.EVENT_COUNT_AFTER_PRUNE;
  }

  private long getNextCleanupTime() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_CLEANUP_TIME, -1L);
  }

  private long getMinimumTimeBetweenCleanupChecks() {
    return HealthReportConstants.MINIMUM_TIME_BETWEEN_CLEANUP_CHECKS_MILLIS;
  }
}
