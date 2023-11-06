/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.process;

import org.mozilla.gecko.annotation.WrapForJNI;

@WrapForJNI
public enum GeckoProcessType {
  // These need to match the stringified names from the GeckoProcessType enum
  PARENT("default"),
  PLUGIN("plugin"),
  CONTENT("tab"),
  IPDLUNITTEST("ipdlunittest"),
  GMPLUGIN("gmplugin"),
  GPU("gpu"),
  VR("vr"),
  RDD("rdd"),
  SOCKET("socket"),
  REMOTESANDBOXBROKER("sandboxbroker"),
  FORKSERVER("forkserver"),
  UTILITY("utility");

  private final String mGeckoName;

  GeckoProcessType(final String geckoName) {
    mGeckoName = geckoName;
  }

  @Override
  public String toString() {
    return mGeckoName;
  }

  @WrapForJNI
  private static GeckoProcessType fromInt(final int type) {
    return values()[type];
  }
}
