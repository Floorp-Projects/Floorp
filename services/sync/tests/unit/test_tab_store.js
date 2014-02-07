/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines/tabs.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services-common/utils.js");

function test_create() {
  let store = new TabEngine(Service)._store;

  _("Create a first record");
  let rec = {id: "id1",
             clientName: "clientName1",
             cleartext: "cleartext1",
             modified: 1000};
  store.applyIncoming(rec);
  do_check_eq(store._remoteClients["id1"], "cleartext1");
  do_check_eq(Svc.Prefs.get("notifyTabState"), 1);

  _("Create a second record");
  let rec = {id: "id2",
             clientName: "clientName2",
             cleartext: "cleartext2",
             modified: 2000};
  store.applyIncoming(rec);
  do_check_eq(store._remoteClients["id2"], "cleartext2");
  do_check_eq(Svc.Prefs.get("notifyTabState"), 0);

  _("Create a third record");
  let rec = {id: "id3",
             clientName: "clientName3",
             cleartext: "cleartext3",
             modified: 3000};
  store.applyIncoming(rec);
  do_check_eq(store._remoteClients["id3"], "cleartext3");
  do_check_eq(Svc.Prefs.get("notifyTabState"), 0);

  // reset the notifyTabState
  Svc.Prefs.reset("notifyTabState");
}

function fakeSessionSvc(url, numtabs) {
  // first delete the getter, or the previously
  // created fake Session
  delete Svc.Session;
  Svc.Session = {
    getBrowserState: function() {
      let obj = {
        windows: [{
          tabs: [{
            index: 1,
            entries: [{
              url: url,
              title: "title"
            }],
            attributes: {
              image: "image"
            },
            lastAccessed: 1499
          }]
        }]
      };
      if (numtabs) {
        let tabs = obj.windows[0].tabs;
        for (let i = 0; i < numtabs-1; i++)
          tabs.push(TestingUtils.deepCopy(tabs[0]));
      }
      return JSON.stringify(obj);
    }
  };
};

function test_getAllTabs() {
  let store = new TabEngine(Service)._store, tabs;

  _("get all tabs");
  fakeSessionSvc("http://foo.com");
  tabs = store.getAllTabs();
  do_check_eq(tabs.length, 1);
  do_check_eq(tabs[0].title, "title");
  do_check_eq(tabs[0].urlHistory.length, 1);
  do_check_eq(tabs[0].urlHistory[0], ["http://foo.com"]);
  do_check_eq(tabs[0].icon, "image");
  do_check_eq(tabs[0].lastUsed, 1);

  _("get all tabs, and check that filtering works");
  // we don't bother testing every URL type here, the
  // filteredUrls regex really should have it own tests
  fakeSessionSvc("about:foo");
  tabs = store.getAllTabs(true);
  do_check_eq(tabs.length, 0);
}

function test_createRecord() {
  let store = new TabEngine(Service)._store, record;

  // get some values before testing
  fakeSessionSvc("http://foo.com");
  let tabs = store.getAllTabs();
  let tabsize = JSON.stringify(tabs[0]).length;
  let numtabs = Math.ceil(20000./77.);

  _("create a record");
  fakeSessionSvc("http://foo.com");
  record = store.createRecord("fake-guid");
  do_check_true(record instanceof TabSetRecord);
  do_check_eq(record.tabs.length, 1);

  _("create a big record");
  fakeSessionSvc("http://foo.com", numtabs);
  record = store.createRecord("fake-guid");
  do_check_true(record instanceof TabSetRecord);
  do_check_eq(record.tabs.length, 256);
}

function run_test() {
  test_create();
  test_getAllTabs();
  test_createRecord();
}
