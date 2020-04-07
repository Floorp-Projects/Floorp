/*
Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/
*/
"use strict";

// This must be loaded in the remote process for this test to be useful
const TEST_URL = "http://example.com/browser/netwerk/test/browser/dummy.html";

const expectedRemote = gMultiProcessBrowser ? "true" : "";

const resProtocol = Cc[
  "@mozilla.org/network/protocol;1?name=resource"
].getService(Ci.nsIResProtocolHandler);

function waitForEvent(obj, name, capturing, chromeEvent) {
  info("Waiting for " + name);
  return new Promise(resolve => {
    function listener(event) {
      info("Saw " + name);
      obj.removeEventListener(name, listener, capturing, chromeEvent);
      resolve(event);
    }

    obj.addEventListener(name, listener, capturing, chromeEvent);
  });
}

function resolveURI(uri) {
  uri = Services.io.newURI(uri);
  try {
    return resProtocol.resolveURI(uri);
  } catch (e) {
    return null;
  }
}

function remoteResolveURI(uri) {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [uri], uriToResolve => {
    const { Services } = ChromeUtils.import(
      "resource://gre/modules/Services.jsm"
    );
    let resProtocol = Cc[
      "@mozilla.org/network/protocol;1?name=resource"
    ].getService(Ci.nsIResProtocolHandler);

    uriToResolve = Services.io.newURI(uriToResolve);
    try {
      return resProtocol.resolveURI(uriToResolve);
    } catch (e) {}
    return null;
  });
}

// Restarts the child process by crashing it then reloading the tab
var restart = async function() {
  let browser = gBrowser.selectedBrowser;
  // If the tab isn't remote this would crash the main process so skip it
  if (browser.getAttribute("remote") != "true") {
    return browser;
  }

  await BrowserTestUtils.crashFrame(browser);

  browser.reload();

  await BrowserTestUtils.browserLoaded(browser);
  is(
    browser.getAttribute("remote"),
    expectedRemote,
    "Browser should be in the right process"
  );
  return browser;
};

// Sanity check that this test is going to be useful
add_task(async function() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  // This must be loaded in the remote process for this test to be useful
  is(
    gBrowser.selectedBrowser.getAttribute("remote"),
    expectedRemote,
    "Browser should be in the right process"
  );

  let local = resolveURI("resource://gre/modules/Services.jsm");
  let remote = await remoteResolveURI("resource://gre/modules/Services.jsm");
  is(local, remote, "Services.jsm should resolve in both processes");

  gBrowser.removeCurrentTab();
});

// Add a mapping, update it then remove it
add_task(async function() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  info("Set");
  resProtocol.setSubstitution(
    "testing",
    Services.io.newURI("chrome://global/content")
  );
  let local = resolveURI("resource://testing/test.js");
  let remote = await remoteResolveURI("resource://testing/test.js");
  is(
    local,
    "chrome://global/content/test.js",
    "Should resolve in main process"
  );
  is(
    remote,
    "chrome://global/content/test.js",
    "Should resolve in child process"
  );

  info("Change");
  resProtocol.setSubstitution(
    "testing",
    Services.io.newURI("chrome://global/skin")
  );
  local = resolveURI("resource://testing/test.js");
  remote = await remoteResolveURI("resource://testing/test.js");
  is(local, "chrome://global/skin/test.js", "Should resolve in main process");
  is(remote, "chrome://global/skin/test.js", "Should resolve in child process");

  info("Clear");
  resProtocol.setSubstitution("testing", null);
  local = resolveURI("resource://testing/test.js");
  remote = await remoteResolveURI("resource://testing/test.js");
  is(local, null, "Shouldn't resolve in main process");
  is(remote, null, "Shouldn't resolve in child process");

  gBrowser.removeCurrentTab();
});

// Add a mapping, restart the child process then check it is still there
add_task(async function() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  info("Set");
  resProtocol.setSubstitution(
    "testing",
    Services.io.newURI("chrome://global/content")
  );
  let local = resolveURI("resource://testing/test.js");
  let remote = await remoteResolveURI("resource://testing/test.js");
  is(
    local,
    "chrome://global/content/test.js",
    "Should resolve in main process"
  );
  is(
    remote,
    "chrome://global/content/test.js",
    "Should resolve in child process"
  );

  await restart();

  local = resolveURI("resource://testing/test.js");
  remote = await remoteResolveURI("resource://testing/test.js");
  is(
    local,
    "chrome://global/content/test.js",
    "Should resolve in main process"
  );
  is(
    remote,
    "chrome://global/content/test.js",
    "Should resolve in child process"
  );

  info("Change");
  resProtocol.setSubstitution(
    "testing",
    Services.io.newURI("chrome://global/skin")
  );

  await restart();

  local = resolveURI("resource://testing/test.js");
  remote = await remoteResolveURI("resource://testing/test.js");
  is(local, "chrome://global/skin/test.js", "Should resolve in main process");
  is(remote, "chrome://global/skin/test.js", "Should resolve in child process");

  info("Clear");
  resProtocol.setSubstitution("testing", null);

  await restart();

  local = resolveURI("resource://testing/test.js");
  remote = await remoteResolveURI("resource://testing/test.js");
  is(local, null, "Shouldn't resolve in main process");
  is(remote, null, "Shouldn't resolve in child process");

  gBrowser.removeCurrentTab();
});

// Adding a mapping to a resource URI should work
add_task(async function() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  info("Set");
  resProtocol.setSubstitution(
    "testing",
    Services.io.newURI("chrome://global/content")
  );
  resProtocol.setSubstitution(
    "testing2",
    Services.io.newURI("resource://testing")
  );
  let local = resolveURI("resource://testing2/test.js");
  let remote = await remoteResolveURI("resource://testing2/test.js");
  is(
    local,
    "chrome://global/content/test.js",
    "Should resolve in main process"
  );
  is(
    remote,
    "chrome://global/content/test.js",
    "Should resolve in child process"
  );

  info("Clear");
  resProtocol.setSubstitution("testing", null);
  local = resolveURI("resource://testing2/test.js");
  remote = await remoteResolveURI("resource://testing2/test.js");
  is(
    local,
    "chrome://global/content/test.js",
    "Should resolve in main process"
  );
  is(
    remote,
    "chrome://global/content/test.js",
    "Should resolve in child process"
  );

  resProtocol.setSubstitution("testing2", null);
  local = resolveURI("resource://testing2/test.js");
  remote = await remoteResolveURI("resource://testing2/test.js");
  is(local, null, "Shouldn't resolve in main process");
  is(remote, null, "Shouldn't resolve in child process");

  gBrowser.removeCurrentTab();
});
