/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.SysInfo;
import org.mozilla.gecko.background.common.log.Logger;

import android.content.ContentProvider;
import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.Context;

/**
 * Construct a HealthReport environment from the current running system.
 */
public class EnvironmentBuilder {
  private static final String LOG_TAG = "GeckoEnvBuilder";

  public static ContentProviderClient getContentProviderClient(Context context) {
    ContentResolver cr = context.getContentResolver();
    return cr.acquireContentProviderClient(HealthReportConstants.HEALTH_AUTHORITY);
  }

  public static HealthReportDatabaseStorage getStorage(ContentProviderClient cpc,
                                                       String profilePath) {
    ContentProvider pr = cpc.getLocalContentProvider();
    try {
      return ((HealthReportProvider) pr).getProfileStorage(profilePath);
    } catch (ClassCastException ex) {
      Logger.error(LOG_TAG, "ContentProvider not a HealthReportProvider!", ex);
      throw ex;
    }
  }

  public static interface ProfileInformationProvider {
    public boolean isBlocklistEnabled();
    public boolean isTelemetryEnabled();
    public long getProfileCreationTime();
  }

  protected static void populateEnvironment(Environment e,
                                            ProfileInformationProvider info) {
    e.cpuCount = SysInfo.getCPUCount();
    e.memoryMB = SysInfo.getMemSize();

    e.appName = AppConstants.MOZ_APP_NAME;
    e.appID = AppConstants.MOZ_APP_ID;
    e.appVersion = AppConstants.MOZ_APP_VERSION;
    e.appBuildID = AppConstants.MOZ_APP_BUILDID;
    e.updateChannel = AppConstants.MOZ_UPDATE_CHANNEL;
    e.vendor = AppConstants.MOZ_APP_VENDOR;
    e.platformVersion = AppConstants.MOZILLA_VERSION;
    e.platformBuildID = AppConstants.MOZ_APP_BUILDID;
    e.xpcomabi = AppConstants.TARGET_XPCOM_ABI;
    e.os = "Android";
    e.architecture = SysInfo.getArchABI();       // Not just "arm".
    e.sysName = SysInfo.getName();
    e.sysVersion = SysInfo.getReleaseVersion();

    e.profileCreation = (int) (info.getProfileCreationTime() / HealthReportConstants.MILLISECONDS_PER_DAY);

    // Corresponds to Gecko pref "extensions.blocklist.enabled".
    e.isBlocklistEnabled = (info.isBlocklistEnabled() ? 1 : 0);

    // Corresponds to one of two Gecko telemetry prefs. We reflect these into
    // GeckoPreferences as "datareporting.telemetry.enabled".
    e.isTelemetryEnabled = (info.isTelemetryEnabled() ? 1 : 0);

    // TODO
    e.extensionCount = 0;
    e.pluginCount = 0;
    e.themeCount = 0;
    // e.addons;

  }

  /**
   * Returns an {@link Environment} not linked to a storage instance, but
   * populated with current field values.
   *
   * @param info a source of profile data
   * @return the new {@link Environment}
   */
  public static Environment getCurrentEnvironment(ProfileInformationProvider info) {
    Environment e = new Environment() {
      @Override
      public int register() {
        return 0;
      }
    };
    populateEnvironment(e, info);
    return e;
  }

  /**
   * @return the current environment's ID in the provided storage layer
   */
  public static int registerCurrentEnvironment(HealthReportDatabaseStorage storage,
                                               ProfileInformationProvider info) {
    Environment e = storage.getEnvironment();
    populateEnvironment(e, info);
    return e.register();
  }
}
