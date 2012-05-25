/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

public class EngineSettings {
  public final String syncID;
  public final int version;

  public EngineSettings(final String syncID, final int version) {
    this.syncID = syncID;
    this.version = version;
  }

  public EngineSettings(ExtendedJSONObject object) {
    try {
      this.syncID = object.getString("syncID");
      this.version = object.getIntegerSafely("version").intValue();
    } catch (Exception e ) {
      throw new IllegalArgumentException(e);
    }
  }

  public ExtendedJSONObject toJSONObject() {
    ExtendedJSONObject json = new ExtendedJSONObject();
    json.put("syncID", syncID);
    json.put("version", version);
    return json;
  }
}
