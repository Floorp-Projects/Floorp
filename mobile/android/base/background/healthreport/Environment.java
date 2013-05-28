/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport;

import java.util.ArrayList;

/**
 * This captures all of the details that define an 'environment' for FHR's purposes.
 * Whenever this format changes, it'll be changing with a build ID, so no migration
 * of values is needed.
 *
 * Unless you remove the build descriptors from the set, of course.
 *
 * Or store these in a database.
 *
 * Instances of this class should be considered "effectively immutable": control their
 * scope such that clear creation/sharing boundaries exist. Once you've populated and
 * registered an <code>Environment</code>, don't do so again; start from scratch.
 *
 */
public abstract class Environment {
  public static int VERSION = 1;

  protected volatile String hash = null;
  protected volatile int id = -1;

  // org.mozilla.profile.age.
  public int profileCreation;

  // org.mozilla.sysinfo.sysinfo.
  public int cpuCount;
  public int memoryMB;
  public String architecture;
  public String sysName;
  public String sysVersion;      // Kernel.

  // geckoAppInfo. Not sure if we can/should provide this on Android.
  public String vendor;
  public String appName;
  public String appID;
  public String appVersion;
  public String appBuildID;
  public String platformVersion;
  public String platformBuildID;
  public String os;
  public String xpcomabi;
  public String updateChannel;

  // appInfo.
  public int isBlocklistEnabled;
  public int isTelemetryEnabled;
  // public int isDefaultBrowser;        // This is meaningless on Android.

  // org.mozilla.addons.active.
  public final ArrayList<String> addons = new ArrayList<String>();

  // org.mozilla.addons.counts.
  public int extensionCount;
  public int pluginCount;
  public int themeCount;

  public String getHash() {
    // It's never unset, so we only care about partial reads. volatile is enough.
    if (hash != null) {
      return hash;
    }

    StringBuilder b = new StringBuilder();
    b.append(profileCreation);
    b.append(cpuCount);
    b.append(memoryMB);
    b.append(architecture);
    b.append(sysName);
    b.append(sysVersion);
    b.append(vendor);
    b.append(appName);
    b.append(appID);
    b.append(appVersion);
    b.append(appBuildID);
    b.append(platformVersion);
    b.append(platformBuildID);
    b.append(os);
    b.append(xpcomabi);
    b.append(updateChannel);
    b.append(isBlocklistEnabled);
    b.append(isTelemetryEnabled);
    b.append(extensionCount);
    b.append(pluginCount);
    b.append(themeCount);

    for (String addon : addons) {
      b.append(addon);
    }

    return hash = HealthReportUtils.getEnvironmentHash(b.toString());
  }

  /**
   * Ensure that the {@link Environment} has been registered with its
   * storage layer, and can be used to annotate events.
   *
   * It's safe to call this method more than once, and each time you'll
   * get the same ID.
   *
   * @return the integer ID to use in subsequent DB insertions.
   */
  public abstract int register();
}
