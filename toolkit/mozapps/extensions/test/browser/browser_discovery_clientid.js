"use strict";

const {ClientID} = ChromeUtils.import("resource://gre/modules/ClientID.jsm");

const MAIN_URL = "https://example.com/" + RELATIVE_DIR + "discovery.html";

// This test is testing XUL about:addons UI (the HTML about:addons is tested in
// browser_html_discover_view_clientid.js).
SpecialPowers.pushPrefEnv({
  set: [
    ["extensions.htmlaboutaddons.discover.enabled", false],
    ["extensions.htmlaboutaddons.enabled", false],
  ],
});

function waitForHeader() {
  return new Promise(resolve => {
    let observer = (subject, topic, state) => {
      let channel = subject.QueryInterface(Ci.nsIHttpChannel);
      if (channel.URI.spec != MAIN_URL) {
        return;
      }
      try {
        resolve(channel.getRequestHeader("Moz-Client-Id"));
      } catch (e) {
        if (e.result == Cr.NS_ERROR_NOT_AVAILABLE) {
          // The header was not set.
          resolve(null);
        }
      } finally {
        Services.obs.removeObserver(observer, "http-on-modify-request");
      }
    };
    Services.obs.addObserver(observer, "http-on-modify-request");
  });
}

add_task(async function setup() {
  SpecialPowers.pushPrefEnv({"set": [
    [PREF_DISCOVERURL, MAIN_URL],
    ["datareporting.healthreport.uploadEnabled", true],
    ["browser.discovery.enabled", true],
  ]});
});

add_task(async function test_no_private_clientid() {
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({private: true});
  let [header, manager] = await Promise.all([
    waitForHeader(),
    open_manager("addons://discover/", undefined, undefined, undefined, privateWindow),
  ]);
  ok(PrivateBrowsingUtils.isContentWindowPrivate(manager), "window is private");
  is(header, null, "header was not set");
  await close_manager(manager);
  await BrowserTestUtils.closeWindow(privateWindow);
});

add_task(async function test_clientid() {
  let clientId = await ClientID.getClientIdHash();
  ok(!!clientId, "clientId is avialable");
  let [header, manager] = await Promise.all([
    waitForHeader(),
    open_manager("addons://discover/"),
  ]);
  is(header, clientId, "header was set");
  await close_manager(manager);
});
