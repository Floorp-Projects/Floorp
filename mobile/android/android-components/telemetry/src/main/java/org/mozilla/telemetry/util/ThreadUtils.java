/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.util;

import android.support.annotation.RestrictTo;

@RestrictTo(RestrictTo.Scope.LIBRARY)
public class ThreadUtils {
    public static Thread runOnThreadAndReturn(String name, Runnable runnable) {
        Thread thread = new Thread(runnable, name);
        thread.start();
        return thread;
    }
}
