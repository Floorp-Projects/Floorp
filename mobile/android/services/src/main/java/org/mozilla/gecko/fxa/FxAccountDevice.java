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
  public static final String JSON_KEY_PUSH_CALLBACK = "pushCallback";
  public static final String JSON_KEY_PUSH_PUBLICKEY = "pushPublicKey";
  public static final String JSON_KEY_PUSH_AUTHKEY = "pushAuthKey";

  public final String id;
  public final String name;
  public final String type;
  public final Boolean isCurrentDevice;
  public final String pushCallback;
  public final String pushPublicKey;
  public final String pushAuthKey;

  public FxAccountDevice(String name, String id, String type, Boolean isCurrentDevice,
                         String pushCallback, String pushPublicKey, String pushAuthKey) {
    this.name = name;
    this.id = id;
    this.type = type;
    this.isCurrentDevice = isCurrentDevice;
    this.pushCallback = pushCallback;
    this.pushPublicKey = pushPublicKey;
    this.pushAuthKey = pushAuthKey;
  }

  public static FxAccountDevice forRegister(String name, String type, String pushCallback,
                                            String pushPublicKey, String pushAuthKey) {
    return new FxAccountDevice(name, null, type, null, pushCallback, pushPublicKey, pushAuthKey);
  }

  public static FxAccountDevice forUpdate(String id, String name, String pushCallback,
                                          String pushPublicKey, String pushAuthKey) {
    return new FxAccountDevice(name, id, null, null, pushCallback, pushPublicKey, pushAuthKey);
  }

  public static FxAccountDevice fromJson(ExtendedJSONObject json) {
    String name = json.getString(JSON_KEY_NAME);
    String id = json.getString(JSON_KEY_ID);
    String type = json.getString(JSON_KEY_TYPE);
    Boolean isCurrentDevice = json.getBoolean(JSON_KEY_ISCURRENTDEVICE);
    String pushCallback = json.getString(JSON_KEY_PUSH_CALLBACK);
    String pushPublicKey = json.getString(JSON_KEY_PUSH_PUBLICKEY);
    String pushAuthKey = json.getString(JSON_KEY_PUSH_AUTHKEY);
    return new FxAccountDevice(name, id, type, isCurrentDevice, pushCallback, pushPublicKey, pushAuthKey);
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
    if (this.pushCallback != null) {
      body.put(JSON_KEY_PUSH_CALLBACK, this.pushCallback);
    }
    if (this.pushPublicKey != null) {
      body.put(JSON_KEY_PUSH_PUBLICKEY, this.pushPublicKey);
    }
    if (this.pushAuthKey != null) {
      body.put(JSON_KEY_PUSH_AUTHKEY, this.pushAuthKey);
    }
    return body;
  }
}
