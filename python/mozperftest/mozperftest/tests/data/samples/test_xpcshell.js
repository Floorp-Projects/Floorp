/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";


add_task(async function dummy_test() {
  /*
   * Do some test here, get some metrics
   */
  var metrics = {"metrics1": 1, "metrics2": 2};
  info("perfMetrics", metrics);
  info("perfMetrics", {"metrics3": 3});
  await true;
});
