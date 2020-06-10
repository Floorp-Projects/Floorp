/* eslint-env node */

async function test(context, commands) {
  "use strict";
  // Nothing to do -- the timing is captured via logcat
  return true;
}

module.exports = {
  test,
  owner: "Performance Team",
  name: "HomeActivity",
  description:
    "Measures the time from process start until the Fenix HomeActivity reports Fully Drawn",
  longDescription: `
  This test launches Fenix to its MAIN activity.
  The application logs "Fully Drawn" when the Home Activity is drawn.
  Using the android log transformer we measure the time from process start to this event.
  `,
  usage: `
    ./mach perftest testing/performance/perftest_home_activity.js --android --flavor mobile-browser \
    --hooks testing/performance/hooks_home_activity.py --perfherder --android-app-name  org.mozilla.fennec_aurora \
    --android-activity .App --android-install-apk ~/Downloads/fenix.apk --android-clear-logcat \
    --android-capture-logcat home_activity_log.txt \
    --androidlog-first-timestamp ".*Start proc.*org\.mozilla\.fenix.*\.App.*" \
    --androidlog-second-timestamp ".*Fully drawn.*org\.mozilla\.fenix.*" \
    --androidlog-subtest-name "MAIN" --androidlog
  `,
  supportedBrowsers: ["Fenix nightly"],
  supportedPlatforms: ["Android"],
};
