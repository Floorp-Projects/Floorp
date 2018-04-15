/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// use httpserver to find an available port
ChromeUtils.import("resource://testing-common/httpd.js");
var gServer = new HttpServer();
gServer.start(-1);
gPort = gServer.identity.primaryPort;

var addon_url = "http://localhost:" + gPort + "/test.xpi";
var icon32_url = "http://localhost:" + gPort + "/icon.png";
var icon64_url = "http://localhost:" + gPort + "/icon64.png";

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  startupManager();

  test_1();
}

async function test_1() {
  let aInstall = await AddonManager.getInstallForURL(addon_url, "application/x-xpinstall", null, null, null, null, null);
  Assert.equal(aInstall.iconURL, null);
  Assert.notEqual(aInstall.icons, null);
  Assert.equal(aInstall.icons[32], undefined);
  Assert.equal(aInstall.icons[64], undefined);
  test_2();
}

async function test_2() {
  let aInstall = await AddonManager.getInstallForURL(addon_url, "application/x-xpinstall", null, null, icon32_url, null, null);
  Assert.equal(aInstall.iconURL, icon32_url);
  Assert.notEqual(aInstall.icons, null);
  Assert.equal(aInstall.icons[32], icon32_url);
  Assert.equal(aInstall.icons[64], undefined);
  test_3();
}

async function test_3() {
  let aInstall = await AddonManager.getInstallForURL(addon_url, "application/x-xpinstall", null, null, { "32": icon32_url }, null, null);
  Assert.equal(aInstall.iconURL, icon32_url);
  Assert.notEqual(aInstall.icons, null);
  Assert.equal(aInstall.icons[32], icon32_url);
  Assert.equal(aInstall.icons[64], undefined);
  test_4();
}

async function test_4() {
  let aInstall = await AddonManager.getInstallForURL(addon_url, "application/x-xpinstall", null, null, { "32": icon32_url, "64": icon64_url }, null, null);
  Assert.equal(aInstall.iconURL, icon32_url);
  Assert.notEqual(aInstall.icons, null);
  Assert.equal(aInstall.icons[32], icon32_url);
  Assert.equal(aInstall.icons[64], icon64_url);
  executeSoon(do_test_finished);
}
