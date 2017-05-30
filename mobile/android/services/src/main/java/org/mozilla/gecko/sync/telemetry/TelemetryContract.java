/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.telemetry;

/**
 * Establishes a common interface between {@link TelemetryCollector} and
 * {@link org.mozilla.gecko.telemetry.TelemetryBackgroundReceiver}.
 */
public class TelemetryContract {
  public final static String KEY_TELEMETRY = "telemetry";
  public final static String KEY_STAGES = "stages";
  public final static String KEY_ERROR = "error";
  public final static String KEY_LOCAL_UID = "uid";
  public final static String KEY_LOCAL_DEVICE_ID = "deviceID";
  public final static String KEY_DEVICES = "devices";
  public final static String KEY_TOOK = "took";
  public final static String KEY_RESTARTED = "restarted";

  public static final String KEY_TYPE = "type";
  public static final String KEY_TYPE_SYNC = "sync";
  public static final String KEY_TYPE_EVENT = "event";

  public static final String KEY_DEVICE_OS = "os";
  public static final String KEY_DEVICE_VERSION = "version";
  public static final String KEY_DEVICE_ID = "id";
}
