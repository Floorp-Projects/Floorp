/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.common.telemetry;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import org.mozilla.gecko.background.common.log.Logger;

/**
 * Android Background Services are normally built into Fennec, but can also be
 * built as a stand-alone APK for rapid local development. The current Telemetry
 * implementation is coupled to Gecko, and Background Services should not
 * interact with Gecko directly. To maintain this independence, Background
 * Services lazily introspects the relevant Telemetry class from the enclosing
 * package, warning but otherwise ignoring failures during introspection or
 * invocation.
 * <p>
 * It is possible that Background Services will introspect and invoke the
 * Telemetry implementation while Gecko is not running. In this case, the Fennec
 * process itself buffers Telemetry events until such time as they can be
 * flushed to disk and uploaded. <b>There is no guarantee that all Telemetry
 * events will be uploaded!</b> Depending on the volume of data and the
 * application lifecycle, Telemetry events may be dropped.
 */
public class TelemetryWrapper {
  private static final String LOG_TAG = TelemetryWrapper.class.getSimpleName();

  // Marking this volatile maintains thread safety cheaply.
  private static volatile Method mAddToHistogram;

  public static void addToHistogram(String key, int value) {
    if (mAddToHistogram == null) {
      try {
        final Class<?> telemetry = Class.forName("org.mozilla.gecko.Telemetry");
        mAddToHistogram = telemetry.getMethod("addToHistogram", String.class, int.class);
      } catch (ClassNotFoundException e) {
        Logger.warn(LOG_TAG, "org.mozilla.gecko.Telemetry class found!");
        return;
      } catch (NoSuchMethodException e) {
        Logger.warn(LOG_TAG, "org.mozilla.gecko.Telemetry.addToHistogram(String, int) method not found!");
        return;
      }
    }

    if (mAddToHistogram != null) {
      try {
        mAddToHistogram.invoke(null, key, value);
      } catch (IllegalArgumentException | InvocationTargetException | IllegalAccessException e) {
        Logger.warn(LOG_TAG, "Got exception invoking telemetry!");
      }
    }
  }
}
