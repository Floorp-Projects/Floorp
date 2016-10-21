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

function makeLinks(links) {
  // Important: To avoid test failures due to clock jitter on Windows XP, call
  // Date.now() once here, not each time through the loop.
  let frecency = 0;
  let now = Date.now() * 1000;
  let places = [];
  links.map((link, i) => {
    places.push({
      url: link.url,
      title: link.title,
      lastVisitDate: now - i,
      frecency: frecency++,
    });
  });
  return places;
}

add_task(function* test_topSites() {
  let expect = [{url: "http://example.com/", title: "site#-1"},
                {url: "http://example0.com/", title: "site#0"},
                {url: "http://example1.com/", title: "site#1"},
                {url: "http://example2.com/", title: "site#2"},
                {url: "http://example3.com/", title: "site#3"}];

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": [
        "topSites",
      ],
    },
    background() {
      browser.topSites.get(result => {
        browser.test.sendMessage("done", result);
      });
    },
  });


  let expectedLinks = makeLinks(expect);
  let provider = new TestProvider(done => done(expectedLinks));

  NewTabUtils.initWithoutProviders();
  NewTabUtils.links.addProvider(provider);

  yield NewTabUtils.links.populateCache();

  yield extension.startup();

  let result = yield extension.awaitMessage("done");
  Assert.deepEqual(expect, result, "got topSites");

  yield extension.unload();

  NewTabUtils.links.removeProvider(provider);
});
