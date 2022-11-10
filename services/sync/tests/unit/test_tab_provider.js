/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { TabProvider } = ChromeUtils.import(
  "resource://services-sync/engines/tabs.js"
);

add_task(async function test_getAllTabs() {
  let provider = TabProvider;
  provider.shouldSkipWindow = mockShouldSkipWindow;

  let tabs;

  provider.getWindowEnumerator = mockGetWindowEnumerator.bind(this, [
    "http://bar.com",
  ]);

  _("Get all tabs.");
  tabs = await provider.getAllTabs();
  _("Tabs: " + JSON.stringify(tabs));
  equal(tabs.length, 1);
  equal(tabs[0].title, "title");
  equal(tabs[0].urlHistory.length, 1);
  equal(tabs[0].urlHistory[0], "http://bar.com/");
  equal(tabs[0].icon, "");
  equal(tabs[0].lastUsed, 2); // windowenumerator returns in ms but the getAllTabs() returns in seconds

  _("Get all tabs, and check that filtering works.");
  provider.getWindowEnumerator = mockGetWindowEnumerator.bind(this, [
    "http://foo.com",
    "about:foo",
  ]);
  tabs = await provider.getAllTabs(true);
  _("Filtered: " + JSON.stringify(tabs));
  equal(tabs.length, 1);

  _("Get all tabs, and check that they are properly sorted");
  provider.getWindowEnumerator = mockGetWindowEnumerator.bind(this, [
    "http://foo.com",
    "http://bar.com",
  ]);
  tabs = await provider.getAllTabs(true);
  _("Ordered: " + JSON.stringify(tabs));
  equal(tabs[0].lastUsed > tabs[1].lastUsed, true);
});
