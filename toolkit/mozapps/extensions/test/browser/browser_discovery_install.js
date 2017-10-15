/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the discovery view can install add-ons correctly

const MAIN_URL = "https://example.com/" + RELATIVE_DIR + "discovery_install.html";
const GOOD_FRAMED_URL = "https://example.com/" + RELATIVE_DIR + "discovery_frame.html";
const BAD_FRAMED_URL = "https://example.org/" + RELATIVE_DIR + "discovery_frame.html";

const PREF_INSTALL_REQUIREBUILTINCERTS = "extensions.install.requireBuiltInCerts";

// Temporarily enable caching
Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
// Allow SSL from non-built-in certs
Services.prefs.setBoolPref(PREF_INSTALL_REQUIREBUILTINCERTS, false);
// Allow installs from the test site
Services.perms.add(NetUtil.newURI("https://example.com/"), "install",
                   Ci.nsIPermissionManager.ALLOW_ACTION);
Services.perms.add(NetUtil.newURI("https://example.org/"), "install",
                   Ci.nsIPermissionManager.ALLOW_ACTION);

registerCleanupFunction(() => {
  Services.perms.remove(NetUtil.newURI("https://example.com/"), "install");
  Services.perms.remove(NetUtil.newURI("https://example.org/"), "install");
  Services.prefs.clearUserPref(PREF_INSTALL_REQUIREBUILTINCERTS);
});

function clickLink(frameLoader, id) {
  let link = frameLoader.contentDocument.getElementById(id);
  EventUtils.sendMouseEvent({type: "click"}, link);
}

function waitForInstall() {
  return new Promise(resolve => {
    wait_for_window_open((window) => {
      is(window.location, "chrome://mozapps/content/xpinstall/xpinstallConfirm.xul",
         "Should have seen the install window");
      window.document.documentElement.cancelDialog();
      resolve();
    });
  });
}

function waitForFail() {
  return new Promise(resolve => {
    let listener = (subject, topic, data) => {
      Services.obs.removeObserver(listener, topic);
      resolve();
    };
    Services.obs.addObserver(listener, "addon-install-origin-blocked");
  });
}

// Tests that navigating to an XPI attempts to install correctly
add_task(async function test_install_direct() {
  Services.prefs.setCharPref(PREF_DISCOVERURL, MAIN_URL);

  let managerWindow = await open_manager("addons://discover/");
  let browser = managerWindow.document.getElementById("discover-browser");

  clickLink(browser, "install-direct");
  await waitForInstall();

  await close_manager(managerWindow);
});

// Tests that installing via JS works correctly
add_task(async function test_install_js() {
  Services.prefs.setCharPref(PREF_DISCOVERURL, MAIN_URL);

  let managerWindow = await open_manager("addons://discover/");
  let browser = managerWindow.document.getElementById("discover-browser");

  clickLink(browser, "install-js");
  await waitForInstall();

  await close_manager(managerWindow);
});

// Installing from an inner-frame of the same origin should work
add_task(async function test_install_inner_direct() {
  Services.prefs.setCharPref(PREF_DISCOVERURL, GOOD_FRAMED_URL);

  let managerWindow = await open_manager("addons://discover/");
  let browser = managerWindow.document.getElementById("discover-browser");
  let frame = browser.contentDocument.getElementById("frame");

  clickLink(frame, "install-direct");
  await waitForInstall();

  await close_manager(managerWindow);
});

add_task(async function test_install_inner_js() {
  Services.prefs.setCharPref(PREF_DISCOVERURL, GOOD_FRAMED_URL);

  let managerWindow = await open_manager("addons://discover/");
  let browser = managerWindow.document.getElementById("discover-browser");
  let frame = browser.contentDocument.getElementById("frame");

  clickLink(frame, "install-js");
  await waitForInstall();

  await close_manager(managerWindow);
});

// Installing from an inner-frame of a different origin should fail
add_task(async function test_install_xorigin_direct() {
  Services.prefs.setCharPref(PREF_DISCOVERURL, BAD_FRAMED_URL);

  let managerWindow = await open_manager("addons://discover/");
  let browser = managerWindow.document.getElementById("discover-browser");
  let frame = browser.contentDocument.getElementById("frame");

  clickLink(frame, "install-direct");
  await waitForFail();

  await close_manager(managerWindow);
});

add_task(async function test_install_xorigin_js() {
  Services.prefs.setCharPref(PREF_DISCOVERURL, BAD_FRAMED_URL);

  let managerWindow = await open_manager("addons://discover/");
  let browser = managerWindow.document.getElementById("discover-browser");
  let frame = browser.contentDocument.getElementById("frame");

  clickLink(frame, "install-js");
  await waitForFail();

  await close_manager(managerWindow);
});
