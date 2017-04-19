/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.devices;

import org.mozilla.gecko.sync.ExtendedJSONObject;

public class FxAccountDevice {

  private static final String JSON_KEY_NAME = "name";
  private static final String JSON_KEY_ID = "id";
  private static final String JSON_KEY_TYPE = "type";
  private static final String JSON_KEY_ISCURRENTDEVICE = "isCurrentDevice";
  private static final String JSON_KEY_PUSH_CALLBACK = "pushCallback";
  private static final String JSON_KEY_PUSH_PUBLICKEY = "pushPublicKey";
  private static final String JSON_KEY_PUSH_AUTHKEY = "pushAuthKey";
  private static final String JSON_LAST_ACCESS_TIME = "lastAccessTime";

  public final String id;
  public final String name;
  public final String type;
  public final Boolean isCurrentDevice;
  public final Long lastAccessTime;
  public final String pushCallback;
  public final String pushPublicKey;
  public final String pushAuthKey;

  public FxAccountDevice(String name, String id, String type, Boolean isCurrentDevice,
                         Long lastAccessTime, String pushCallback, String pushPublicKey,
                         String pushAuthKey) {
    this.name = name;
    this.id = id;
    this.type = type;
    this.isCurrentDevice = isCurrentDevice;
    this.lastAccessTime = lastAccessTime;
    this.pushCallback = pushCallback;
    this.pushPublicKey = pushPublicKey;
    this.pushAuthKey = pushAuthKey;
  }

  public static FxAccountDevice fromJson(ExtendedJSONObject json) {
    final String name = json.getString(JSON_KEY_NAME);
    final String id = json.getString(JSON_KEY_ID);
    final String type = json.getString(JSON_KEY_TYPE);
    final Boolean isCurrentDevice = json.getBoolean(JSON_KEY_ISCURRENTDEVICE);
    final Long lastAccessTime = json.getLong(JSON_LAST_ACCESS_TIME);
    final String pushCallback = json.getString(JSON_KEY_PUSH_CALLBACK);
    final String pushPublicKey = json.getString(JSON_KEY_PUSH_PUBLICKEY);
    final String pushAuthKey = json.getString(JSON_KEY_PUSH_AUTHKEY);
    return new FxAccountDevice(name, id, type, isCurrentDevice, lastAccessTime, pushCallback,
                               pushPublicKey, pushAuthKey);
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

  public static class Builder {
    private String id;
    private String name;
    private String type;
    private String pushCallback;
    private String pushPublicKey;
    private String pushAuthKey;

    public void id(String id) {
      this.id = id;
    }

    public void name(String name) {
      this.name = name;
    }

    public void type(String type) {
      this.type = type;
    }

    public void pushCallback(String pushCallback) {
      this.pushCallback = pushCallback;
    }

    public void pushPublicKey(String pushPublicKey) {
      this.pushPublicKey = pushPublicKey;
    }

    public void pushAuthKey(String pushAuthKey) {
      this.pushAuthKey = pushAuthKey;
    }

    public FxAccountDevice build() {
      return new FxAccountDevice(this.name, this.id, this.type, null, null,
                                 this.pushCallback, this.pushPublicKey, this.pushAuthKey);
    }
  }
}
