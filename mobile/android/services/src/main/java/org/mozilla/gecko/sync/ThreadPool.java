/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class ThreadPool {
  public static final ExecutorService executorService = Executors.newCachedThreadPool();
  public static void run(Runnable runnable) {
    executorService.submit(runnable);
  }
}
