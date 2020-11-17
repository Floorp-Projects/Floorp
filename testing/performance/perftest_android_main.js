/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

async function test(context, commands) {
  "use strict";
  // Nothing to do -- the timing is captured via logcat
  return true;
}

module.exports = {
  test,
  owner: "Performance Team",
  name: "main",
  description:
    "Measures the time from process start until the Fenix main activity (HomeActivity) reports Fully Drawn",
  longDescription: `
  This test launches Fenix to its main activity (HomeActivity).
  The application logs "Fully Drawn" when the activity is drawn.
  Using the android log transformer we measure the time from process start to this event.
  `,
  usage: `
    ./mach perftest testing/performance/perftest_android_main.js --android --flavor mobile-browser \
    --hooks testing/performance/hooks_home_activity.py --perfherder --android-app-name  org.mozilla.fenix \
    --android-activity .App --android-install-apk ~/Downloads/fenix.apk --android-clear-logcat \
    --android-capture-logcat logcat \
    --androidlog-first-timestamp ".*Start proc.*org\.mozilla\.fenix.*\.App.*" \
    --androidlog-second-timestamp ".*Fully drawn.*org\.mozilla\.fenix.*" \
    --androidlog-subtest-name "MAIN" --androidlog
  `,
  supportedBrowsers: ["Fenix nightly"],
  supportedPlatforms: ["Android"],
};
