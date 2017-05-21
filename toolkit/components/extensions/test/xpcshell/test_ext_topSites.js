"use strict";

Cu.import("resource://gre/modules/NewTabUtils.jsm");


function TestProvider(getLinksFn) {
  this.getLinks = getLinksFn;
  this._observers = new Set();
}

TestProvider.prototype = {
  addObserver: function(observer) {
    this._observers.add(observer);
  },
  notifyLinkChanged: function(link, index = -1, deleted = false) {
    this._notifyObservers("onLinkChanged", link, index, deleted);
  },
  notifyManyLinksChanged: function() {
    this._notifyObservers("onManyLinksChanged");
  },
  _notifyObservers: function(observerMethodName, ...args) {
    args.unshift(this);
    for (let obs of this._observers) {
      if (obs[observerMethodName]) {
        obs[observerMethodName].apply(NewTabUtils.links, args);
      }
    }
  },
};

add_task(async function test_topSites() {
  // Important: To avoid test failures due to clock jitter on Windows XP, call
  // Date.now() once here, not each time through the loop.
  let now = Date.now() * 1000;
  let provider1 = new TestProvider(done => {
    let data = [{url: "http://example.com/", title: "site#-1", frecency: 9, lastVisitDate: now},
                {url: "http://example0.com/", title: "site#0", frecency: 8, lastVisitDate: now},
                {url: "http://example3.com/", title: "site#3", frecency: 5, lastVisitDate: now}];
    done(data);
  });
  let provider2 = new TestProvider(done => {
    let data = [{url: "http://example1.com/", title: "site#1", frecency: 7, lastVisitDate: now},
                {url: "http://example2.com/", title: "site#2", frecency: 6, lastVisitDate: now}];
    done(data);
  });

  NewTabUtils.initWithoutProviders();
  NewTabUtils.links.addProvider(provider1);
  NewTabUtils.links.addProvider(provider2);
  NewTabUtils.test1Provider = provider1;
  NewTabUtils.test2Provider = provider2;

  // Test that results from all providers are returned by default.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": [
        "topSites",
      ],
    },
    background() {
      // Tests consistent behaviour when no providers are specified.
      browser.topSites.get(result => {
        browser.test.sendMessage("done1", result);
      });
      browser.topSites.get({}, result => {
        browser.test.sendMessage("done2", result);
      });
      browser.topSites.get({providers: []}, result => {
        browser.test.sendMessage("done3", result);
      });
      // Tests that results are merged correctly.
      browser.topSites.get({providers: ["test2", "test1"]}, result => {
        browser.test.sendMessage("done4", result);
      });
      // Tests that only the specified provider is used.
      browser.topSites.get({providers: ["test2"]}, result => {
        browser.test.sendMessage("done5", result);
      });
      // Tests that specifying a non-existent provider returns an empty array.
      browser.topSites.get({providers: ["fake"]}, result => {
        browser.test.sendMessage("done6", result);
      });
    },
  });

  await extension.startup();

  let expected1 = [{url: "http://example.com/", title: "site#-1"},
                   {url: "http://example0.com/", title: "site#0"},
                   {url: "http://example1.com/", title: "site#1"},
                   {url: "http://example2.com/", title: "site#2"},
                   {url: "http://example3.com/", title: "site#3"}];

  let actual1 = await extension.awaitMessage("done1");
  Assert.deepEqual(expected1, actual1, "got topSites");

  let actual2 = await extension.awaitMessage("done2");
  Assert.deepEqual(expected1, actual2, "got topSites");

  let actual3 = await extension.awaitMessage("done3");
  Assert.deepEqual(expected1, actual3, "got topSites");

  let actual4 = await extension.awaitMessage("done4");
  Assert.deepEqual(expected1, actual4, "got topSites");

  let expected5 = [{url: "http://example1.com/", title: "site#1"},
                   {url: "http://example2.com/", title: "site#2"}];

  let actual5 = await extension.awaitMessage("done5");
  Assert.deepEqual(expected5, actual5, "got topSites");

  let actual6 = await extension.awaitMessage("done6");
  Assert.deepEqual([], actual6, "got topSites");

  await extension.unload();

  NewTabUtils.links.removeProvider(provider1);
  NewTabUtils.links.removeProvider(provider2);
  delete NewTabUtils.test1Provider;
  delete NewTabUtils.test2Provider;
});
