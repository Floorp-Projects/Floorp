/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PREF_BLOCKLIST_URL              = "extensions.blocklist.url";

add_task(async function test_switch_blocklist_implementations() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  // In this folder, `useXML` is already set, and set to false, ie we're using remote settings.
  Services.prefs.setCharPref(PREF_BLOCKLIST_URL, "http://localhost/blocklist.xml");
  let {Blocklist} = ChromeUtils.import("resource://gre/modules/Blocklist.jsm");
  await Blocklist.loadBlocklistAsync();
  // Observe request:
  let sentRequest = TestUtils.topicObserved("http-on-modify-request", function check(subject, data) {
    let httpChannel = subject.QueryInterface(Ci.nsIHttpChannel);
    info("Got request for: " + httpChannel.URI.spec);
    return httpChannel.URI.spec.startsWith("http://localhost/blocklist.xml");
  });
  // Switch to XML:
  Services.prefs.setBoolPref("extensions.blocklist.useXML", true);
  // Check we're updating:
  await sentRequest;
  ok(true, "We got an update request for the XML list when switching");

  // Now do the same for the remote settings list:
  let {Utils} = ChromeUtils.import("resource://services-settings/Utils.jsm");
  // Mock the 'get latest changes' endpoint so we don't need a network request for it:
  Utils.fetchLatestChanges = () => Promise.resolve({changes: [{last_modified: Date.now()}]});
  let requestPromises = ["addons", "plugins", "gfx"].map(slug => {
    return TestUtils.topicObserved("http-on-modify-request", function check(subject, data) {
      let httpChannel = subject.QueryInterface(Ci.nsIHttpChannel);
      info("Got request for: " + httpChannel.URI.spec);
      return httpChannel.URI.spec.includes("buckets/blocklists/collections/" + slug);
    });
  });
  Services.prefs.setBoolPref("extensions.blocklist.useXML", false);
  await Promise.all(requestPromises);
  ok(true, "We got update requests for all the remote settings lists when switching.");
});
