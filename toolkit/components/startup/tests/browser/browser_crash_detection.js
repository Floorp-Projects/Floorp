/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  function checkLastSuccess() {
    let lastSuccess = Services.prefs.getIntPref("toolkit.startup.last_success");
    let si = Services.startup.getStartupInfo();
    is(lastSuccess, parseInt(si.main.getTime() / 1000, 10),
       "Startup tracking pref should be set after a delay at the end of startup");
    finish();
  }

  if (Services.prefs.getPrefType("toolkit.startup.max_resumed_crashes") == Services.prefs.PREF_INVALID) {
    info("Skipping this test since startup crash detection is disabled");
    return;
  }

  const startupCrashEndDelay = 35 * 1000;
  waitForExplicitFinish();
  requestLongerTimeout(2);
  setTimeout(checkLastSuccess, startupCrashEndDelay);
}
