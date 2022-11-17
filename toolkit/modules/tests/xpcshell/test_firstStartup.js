"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
const { FirstStartup } = ChromeUtils.importESModule(
  "resource://gre/modules/FirstStartup.sys.mjs"
);
const { updateAppInfo } = ChromeUtils.importESModule(
  "resource://testing-common/AppInfo.sys.mjs"
);

const PREF_TIMEOUT = "first-startup.timeout";

add_task(async function test_success() {
  updateAppInfo();
  FirstStartup.init();
  if (AppConstants.MOZ_NORMANDY || AppConstants.MOZ_UPDATE_AGENT) {
    equal(FirstStartup.state, FirstStartup.SUCCESS);
  } else {
    equal(FirstStartup.state, FirstStartup.UNSUPPORTED);
  }
});

add_task(async function test_timeout() {
  updateAppInfo();
  Services.prefs.setIntPref(PREF_TIMEOUT, 0);
  FirstStartup.init();

  if (AppConstants.MOZ_NORMANDY || AppConstants.MOZ_UPDATE_AGENT) {
    equal(FirstStartup.state, FirstStartup.TIMED_OUT);
  } else {
    equal(FirstStartup.state, FirstStartup.UNSUPPORTED);
  }
});
