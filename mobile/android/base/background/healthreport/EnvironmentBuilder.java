/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport;

import java.util.Iterator;

import org.json.JSONObject;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.SysInfo;
import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.healthreport.Environment.UIType;

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

  /**
   * Fetch the storage object associated with the provided
   * {@link ContentProviderClient}. If no storage instance can be found --
   * perhaps because the {@link ContentProvider} is running in a different
   * process -- returns <code>null</code>. On success, the returned
   * {@link HealthReportDatabaseStorage} instance is owned by the underlying
   * {@link HealthReportProvider} and thus does not need to be closed by the
   * caller.
   *
   * If the provider is not a {@link HealthReportProvider}, throws a
   * {@link ClassCastException}, because that would be disastrous.
   */
  public static HealthReportDatabaseStorage getStorage(ContentProviderClient cpc,
                                                       String profilePath) {
    ContentProvider pr = cpc.getLocalContentProvider();
    if (pr == null) {
      Logger.error(LOG_TAG, "Unable to retrieve local content provider. Running in a different process?");
      return null;
    }
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
    public boolean isAcceptLangUserSet();
    public long getProfileCreationTime();

    public String getDistributionString();
    public String getOSLocale();
    public String getAppLocale();

    public JSONObject getAddonsJSON();
  }

  public static interface ConfigurationProvider {
    public boolean hasHardwareKeyboard();

    public UIType getUIType();
    public int getUIModeType();

    public int getScreenLayoutSize();
    public int getScreenXInMM();
    public int getScreenYInMM();
  }

  protected static void populateEnvironment(Environment e,
                                            ProfileInformationProvider info,
                                            ConfigurationProvider config) {
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

    e.profileCreation = (int) (info.getProfileCreationTime() / GlobalConstants.MILLISECONDS_PER_DAY);

    // Corresponds to Gecko pref "extensions.blocklist.enabled".
    e.isBlocklistEnabled = (info.isBlocklistEnabled() ? 1 : 0);

    // Corresponds to Gecko pref "toolkit.telemetry.enabled".
    e.isTelemetryEnabled = (info.isTelemetryEnabled() ? 1 : 0);

    e.extensionCount = 0;
    e.pluginCount = 0;
    e.themeCount = 0;

    JSONObject addons = info.getAddonsJSON();
    if (addons != null) {
      @SuppressWarnings("unchecked")
      Iterator<String> it = addons.keys();
      while (it.hasNext()) {
        String key = it.next();
        try {
          JSONObject addon = addons.getJSONObject(key);
          String type = addon.optString("type");
          Logger.pii(LOG_TAG, "Add-on " + key + " is a " + type);
          if ("extension".equals(type)) {
            ++e.extensionCount;
          } else if ("plugin".equals(type)) {
            ++e.pluginCount;
          } else if ("theme".equals(type)) {
            ++e.themeCount;
          } else if ("service".equals(type)) {
            // Later.
          } else {
            Logger.debug(LOG_TAG, "Unknown add-on type: " + type);
          }
        } catch (Exception ex) {
          Logger.warn(LOG_TAG, "Failed to process add-on " + key, ex);
        }
      }
    }

    e.addons = addons;

    // v2 environment fields.
    e.distribution = info.getDistributionString();
    e.osLocale = info.getOSLocale();
    e.appLocale = info.getAppLocale();
    e.acceptLangSet = info.isAcceptLangUserSet() ? 1 : 0;

    // v3 environment fields.
    e.hasHardwareKeyboard = config.hasHardwareKeyboard();
    e.uiType = config.getUIType();
    e.uiMode = config.getUIModeType();
    e.screenLayout = config.getScreenLayoutSize();
    e.screenXInMM = config.getScreenXInMM();
    e.screenYInMM = config.getScreenYInMM();
  }

  /**
   * Returns an {@link Environment} not linked to a storage instance, but
   * populated with current field values.
   *
   * @param info a source of profile data
   * @return the new {@link Environment}
   */
  public static Environment getCurrentEnvironment(ProfileInformationProvider info, ConfigurationProvider config) {
    Environment e = new Environment() {
      @Override
      public int register() {
        return 0;
      }
    };
    populateEnvironment(e, info, config);
    return e;
  }

  /**
   * @return the current environment's ID in the provided storage layer
   */
  public static int registerCurrentEnvironment(final HealthReportStorage storage,
                                               final ProfileInformationProvider info,
                                               final ConfigurationProvider config) {
    Environment e = storage.getEnvironment();
    populateEnvironment(e, info, config);
    e.register();
    Logger.debug(LOG_TAG, "Registering current environment: " + e.getHash() + " = " + e.id);
    return e.id;
  }
}
