/* exported startup, shutdown, install, ADDON_ID */
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/AddonManager.jsm");

const ADDON_ID = "test_delay_update_complete@tests.mozilla.org";

function install(data, reason) {}

function startup(data, reason) {}

function shutdown(data, reason) {}
