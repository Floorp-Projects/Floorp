"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { FirstStartup } = ChromeUtils.import(
  "resource://gre/modules/FirstStartup.jsm"
);
const { updateAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
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
