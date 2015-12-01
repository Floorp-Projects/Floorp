/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport.prune;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.healthreport.AndroidConfigurationProvider;
import org.mozilla.gecko.background.healthreport.Environment;
import org.mozilla.gecko.background.healthreport.EnvironmentBuilder;
import org.mozilla.gecko.background.healthreport.EnvironmentBuilder.ConfigurationProvider;
import org.mozilla.gecko.background.healthreport.HealthReportDatabaseStorage;
import org.mozilla.gecko.background.healthreport.ProfileInformationCache;

import android.content.ContentProviderClient;
import android.content.Context;

/**
 * Abstracts over the Storage instance behind the PrunePolicy. The underlying storage instance is
 * a {@link HealthReportDatabaseStorage} instance. Since our cleanup routine vacuums, auto_vacuum
 * can be disabled. It is enabled by default, however, turning it off requires an expensive vacuum
 * so we wait until our first {@link cleanup} call since we are vacuuming anyway.
 */
public class PrunePolicyDatabaseStorage implements PrunePolicyStorage {
  public static final String LOG_TAG = PrunePolicyDatabaseStorage.class.getSimpleName();

  private final Context context;
  private final String profilePath;
  private final ConfigurationProvider config;

  private ContentProviderClient client;
  private HealthReportDatabaseStorage storage;

  private int currentEnvironmentID; // So we don't prune the current environment.

  public PrunePolicyDatabaseStorage(final Context context, final String profilePath) {
    this.context = context;
    this.profilePath = profilePath;
    this.config = new AndroidConfigurationProvider(context);

    this.currentEnvironmentID = -1;
  }

  @Override
  public void pruneEvents(final int count) {
    getStorage().pruneEvents(count);
  }

  @Override
  public void pruneEnvironments(final int count) {
    getStorage().pruneEnvironments(count);

    // Re-populate the DB and environment cache with the current environment in the unlikely event
    // that it was deleted.
    this.currentEnvironmentID = -1;
    getCurrentEnvironmentID();
  }

  /**
   * Deletes data recorded before the given time. Note that if this method fails to retrieve the
   * current environment from the profile cache, it will not delete data so be sure to prune by
   * other methods (e.g. {@link pruneEvents}) as well.
   */
  @Override
  public int deleteDataBefore(final long time) {
    return getStorage().deleteDataBefore(time, getCurrentEnvironmentID());
  }

  @Override
  public void cleanup() {
    final HealthReportDatabaseStorage storage = getStorage();
    // The change to auto_vacuum will only take affect after a vacuum.
    storage.disableAutoVacuuming();
    storage.vacuum();
  }

  @Override
  public int getEventCount() {
    return getStorage().getEventCount();
  }

  @Override
  public int getEnvironmentCount() {
    return getStorage().getEnvironmentCount();
  }

  @Override
  public void close() {
    if (client != null) {
      client.release();
      client = null;
    }
  }

  /**
   * Retrieves the {@link HealthReportDatabaseStorage} associated with the profile of the policy.
   * For efficiency, the underlying {@link ContentProviderClient} and
   * {@link HealthReportDatabaseStorage} are cached for later invocations. However, this means a
   * call to this method MUST be accompanied by a call to {@link close}. Throws
   * {@link IllegalStateException} if the storage instance could not be retrieved - note that the
   * {@link ContentProviderClient} instance will not be closed in this case and
   * {@link releaseClient} should still be called.
   */
  protected HealthReportDatabaseStorage getStorage() {
    if (storage != null) {
      return storage;
    }

    client = EnvironmentBuilder.getContentProviderClient(context);
    if (client == null) {
      // TODO: Record prune failures and submit as part of FHR upload.
      Logger.warn(LOG_TAG, "Unable to get ContentProviderClient - throwing.");
      throw new IllegalStateException("Unable to get ContentProviderClient.");
    }

    try {
      storage = EnvironmentBuilder.getStorage(client, profilePath);
      if (storage == null) {
        // TODO: Record prune failures and submit as part of FHR upload.
        Logger.warn(LOG_TAG,"Unable to get HealthReportDatabaseStorage for " + profilePath +
            " - throwing.");
        throw new IllegalStateException("Unable to get HealthReportDatabaseStorage for " +
            profilePath + " (== null).");
      }
    } catch (ClassCastException ex) {
      // TODO: Record prune failures and submit as part of FHR upload.
      Logger.warn(LOG_TAG,"Unable to get HealthReportDatabaseStorage for " + profilePath +
          profilePath + " (ClassCastException).");
      throw new IllegalStateException("Unable to get HealthReportDatabaseStorage for " +
          profilePath + ".", ex);
    }

    return storage;
  }

  protected int getCurrentEnvironmentID() {
    if (currentEnvironmentID < 0) {
      final ProfileInformationCache cache = new ProfileInformationCache(profilePath);
      if (!cache.restoreUnlessInitialized()) {
        throw new IllegalStateException("Current environment unknown.");
      }
      final Environment env = EnvironmentBuilder.getCurrentEnvironment(cache, config);
      currentEnvironmentID = env.register();
    }
    return currentEnvironmentID;
  }
}
