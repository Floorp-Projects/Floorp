/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa;

import org.mozilla.gecko.sync.ExtendedJSONObject;

public class FxAccountDevice {

  public static final String JSON_KEY_NAME = "name";
  public static final String JSON_KEY_ID = "id";
  public static final String JSON_KEY_TYPE = "type";
  public static final String JSON_KEY_ISCURRENTDEVICE = "isCurrentDevice";

  public String id;
  public String name;
  public String type;
  public Boolean isCurrentDevice;

  public FxAccountDevice(String name, String id, String type, Boolean isCurrentDevice) {
    this.name = name;
    this.id = id;
    this.type = type;
    this.isCurrentDevice = isCurrentDevice;
  }

  public static FxAccountDevice forRegister(String name, String type) {
    return new FxAccountDevice(name, null, type, null);
  }

  public static FxAccountDevice forUpdate(String id, String name) {
    return new FxAccountDevice(name, id, null, null);
  }

  public static FxAccountDevice fromJson(ExtendedJSONObject json) {
    String name = json.getString(JSON_KEY_NAME);
    String id = json.getString(JSON_KEY_ID);
    String type = json.getString(JSON_KEY_TYPE);
    Boolean isCurrentDevice = json.getBoolean(JSON_KEY_ISCURRENTDEVICE);
    return new FxAccountDevice(name, id, type, isCurrentDevice);
  }

  public ExtendedJSONObject toJson() {
    final ExtendedJSONObject body = new ExtendedJSONObject();
    if (this.name != null) {
      body.put(JSON_KEY_NAME, this.name);
    }
    if (this.id != null) {
      body.put(JSON_KEY_ID, this.id);
    }
    if (this.type != null) {
      body.put(JSON_KEY_TYPE, this.type);
    }
    return body;
  }
}
