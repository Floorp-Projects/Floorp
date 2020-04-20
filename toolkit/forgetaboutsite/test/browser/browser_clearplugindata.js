/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

let { ForgetAboutSite } = ChromeUtils.import(
  "resource://gre/modules/ForgetAboutSite.jsm"
);

// Test clearing plugin data by domain using ForgetAboutSite.
const testURL =
  "http://mochi.test:8888/browser/toolkit/forgetaboutsite/test/browser/browser_clearplugindata.html";

const pluginHostIface = Ci.nsIPluginHost;
var pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
pluginHost.QueryInterface(pluginHostIface);

var pluginTag;

function stored(needles) {
  var something = pluginHost.siteHasData(this.pluginTag, null);
  if (!needles) {
    return something;
  }

  if (!something) {
    return false;
  }

  for (var i = 0; i < needles.length; ++i) {
    if (!pluginHost.siteHasData(this.pluginTag, needles[i])) {
      return false;
    }
  }
  return true;
}

function setTestPluginEnabledState(newEnabledState, plugin) {
  var oldEnabledState = plugin.enabledState;
  plugin.enabledState = newEnabledState;
  SimpleTest.registerCleanupFunction(function() {
    plugin.enabledState = oldEnabledState;
  });
}

add_task(async function() {
  var tags = pluginHost.getPluginTags();

  // Find the test plugin
  for (var i = 0; i < tags.length; i++) {
    if (tags[i].name == "Test Plug-in") {
      pluginTag = tags[i];
    }
  }
  if (!pluginTag) {
    ok(false, "Test Plug-in not available, can't run test");
    return;
  }
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, pluginTag);
  await BrowserTestUtils.openNewForegroundTab(gBrowser, testURL);

  // Set data for the plugin after the page load.
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    content.wrappedJSObject.testSteps();
  });

  ok(
    stored([
      "192.168.1.1",
      "foo.com",
      "nonexistent.foo.com",
      "bar.com",
      "localhost",
    ]),
    "Data stored for sites"
  );

  // Clear data for "foo.com" and its subdomains.
  await ForgetAboutSite.removeDataFromDomain("foo.com");

  ok(stored(["bar.com", "192.168.1.1", "localhost"]), "Data stored for sites");
  ok(!stored(["foo.com"]), "Data cleared for foo.com");
  ok(!stored(["bar.foo.com"]), "Data cleared for subdomains of foo.com");

  // Clear data for "bar.com" using a subdomain.
  await ForgetAboutSite.removeDataFromDomain("foo.bar.com");
  ok(!stored(["bar.com"]), "Data cleared for bar.com");

  // Clear data for "192.168.1.1".
  await ForgetAboutSite.removeDataFromDomain("192.168.1.1");
  ok(!stored(["192.168.1.1"]), "Data cleared for 192.168.1.1");

  // Clear data for "localhost".
  await ForgetAboutSite.removeDataFromDomain("localhost");
  ok(!stored(null), "All data cleared");

  gBrowser.removeCurrentTab();
});
