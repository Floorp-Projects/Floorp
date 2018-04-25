/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry;

import java.util.concurrent.ExecutionException;

public class TestUtils {
    /**
     * Wait for the internal executor service to execute all scheduled runnables.
     */
    public static void waitForExecutor(Telemetry telemetry) throws ExecutionException, InterruptedException {
        telemetry.getExecutor().submit(new Runnable() {
            @Override
            public void run() {

            }
        }).get();
    }
}
