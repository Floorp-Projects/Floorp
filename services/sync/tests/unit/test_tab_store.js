/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { TabEngine, TabSetRecord } = ChromeUtils.import(
  "resource://services-sync/engines/tabs.js"
);
const { Service } = ChromeUtils.import("resource://services-sync/service.js");

async function getMockStore() {
  let engine = new TabEngine(Service);
  await engine.initialize();
  let store = engine._store;
  store.getTabState = mockGetTabState;
  store.shouldSkipWindow = mockShouldSkipWindow;
  return store;
}

add_task(async function test_create() {
  let engine = new TabEngine(Service);
  await engine.initialize();
  let store = engine._store;

  _("Create a first record");
  let rec = {
    id: "id1",
    clientName: "clientName1",
    cleartext: { foo: "bar" },
    modified: 1000,
  };
  await store.applyIncoming(rec);
  deepEqual(store._remoteClients.id1, { lastModified: 1000, foo: "bar" });

  _("Create a second record");
  rec = {
    id: "id2",
    clientName: "clientName2",
    cleartext: { foo2: "bar2" },
    modified: 2000,
  };
  await store.applyIncoming(rec);
  deepEqual(store._remoteClients.id2, { lastModified: 2000, foo2: "bar2" });

  _("Create a third record");
  rec = {
    id: "id3",
    clientName: "clientName3",
    cleartext: { foo3: "bar3" },
    modified: 3000,
  };
  await store.applyIncoming(rec);
  deepEqual(store._remoteClients.id3, { lastModified: 3000, foo3: "bar3" });
});

add_task(async function test_getAllTabs() {
  let store = await getMockStore();
  let tabs;

  let threeUrls = ["http://foo.com", "http://fuubar.com", "http://barbar.com"];

  store.getWindowEnumerator = mockGetWindowEnumerator.bind(
    this,
    "http://bar.com",
    1,
    1,
    () => 2,
    () => threeUrls
  );

  _("Get all tabs.");
  tabs = await store.getAllTabs();
  _("Tabs: " + JSON.stringify(tabs));
  equal(tabs.length, 1);
  equal(tabs[0].title, "title");
  equal(tabs[0].urlHistory.length, 2);
  equal(tabs[0].urlHistory[0], "http://foo.com");
  equal(tabs[0].urlHistory[1], "http://bar.com");
  equal(tabs[0].icon, "");
  equal(tabs[0].lastUsed, 1);

  _("Get all tabs, and check that filtering works.");
  let twoUrls = ["about:foo", "http://fuubar.com"];
  store.getWindowEnumerator = mockGetWindowEnumerator.bind(
    this,
    "http://foo.com",
    1,
    1,
    () => 2,
    () => twoUrls
  );
  tabs = await store.getAllTabs(true);
  _("Filtered: " + JSON.stringify(tabs));
  equal(tabs.length, 0);

  _("Get all tabs, and check that the entries safety limit works.");
  let allURLs = [];
  for (let i = 0; i < 50; i++) {
    allURLs.push("http://foo" + i + ".bar");
  }
  allURLs.splice(35, 0, "about:foo", "about:bar", "about:foobar");

  store.getWindowEnumerator = mockGetWindowEnumerator.bind(
    this,
    "http://bar.com",
    1,
    1,
    () => 45,
    () => allURLs
  );
  tabs = await store.getAllTabs(url => url.startsWith("about"));

  _("Sliced: " + JSON.stringify(tabs));
  equal(tabs.length, 1);
  equal(tabs[0].urlHistory.length, 5);
  equal(tabs[0].urlHistory[0], "http://foo40.bar");
  equal(tabs[0].urlHistory[4], "http://foo36.bar");
});

add_task(async function test_createRecord() {
  let store = await getMockStore();
  let record;

  store.getTabState = mockGetTabState;
  store.shouldSkipWindow = mockShouldSkipWindow;
  store.getWindowEnumerator = mockGetWindowEnumerator.bind(
    this,
    "http://foo.com",
    1,
    1
  );
  // This number is sensitive to our hard-coded default max record payload size
  // in service.js (256 * 1024).
  // It should be larger than how many records we can fit in a single payload.
  let numtabs = 2700;

  store.getWindowEnumerator = mockGetWindowEnumerator.bind(
    this,
    "http://foo.com",
    1,
    1
  );
  record = await store.createRecord("fake-guid");
  ok(record instanceof TabSetRecord);
  equal(record.tabs.length, 1);

  _("create a big record");
  store.getWindowEnumerator = mockGetWindowEnumerator.bind(
    this,
    "http://foo.com",
    1,
    numtabs
  );
  record = await store.createRecord("fake-guid");
  ok(record instanceof TabSetRecord);
  // This number is sensitive to our hard-coded default max record payload size
  // in service.js (256 * 1024). Given our mock session-store etc, it is the
  // actual max we can fit.
  equal(record.tabs.length, 2672);

  let maxSizeStub = sinon
    .stub(Service, "getMemcacheMaxRecordPayloadSize")
    .callsFake(() => 512 * 1024);
  try {
    numtabs = 5400;
    _("Modify the max record payload size and create a big record");
    store.getWindowEnumerator = mockGetWindowEnumerator.bind(
      this,
      "http://foo.com",
      1,
      numtabs
    );
    record = await store.createRecord("fake-guid");
    ok(record instanceof TabSetRecord);
    equal(record.tabs.length, 5365);
  } finally {
    maxSizeStub.restore();
  }
});
