/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// See also browser/base/content/test/newtab/.

var { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;
Cu.import("resource://gre/modules/NewTabUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const PREF_NEWTAB_ENHANCED = "browser.newtabpage.enhanced";

function run_test() {
  Services.prefs.setBoolPref(PREF_NEWTAB_ENHANCED, true);
  run_next_test();
}

add_task(function* validCacheMidPopulation() {
  let expectedLinks = makeLinks(0, 3, 1);

  let provider = new TestProvider(done => done(expectedLinks));
  provider.maxNumLinks = expectedLinks.length;

  NewTabUtils.initWithoutProviders();
  NewTabUtils.links.addProvider(provider);
  let promise = new Promise(resolve => NewTabUtils.links.populateCache(resolve));

  // isTopSiteGivenProvider() and getProviderLinks() should still return results
  // even when cache is empty or being populated.
  do_check_false(NewTabUtils.isTopSiteGivenProvider("example1.com", provider));
  do_check_links(NewTabUtils.getProviderLinks(provider), []);

  yield promise;

  // Once the cache is populated, we get the expected results
  do_check_true(NewTabUtils.isTopSiteGivenProvider("example1.com", provider));
  do_check_links(NewTabUtils.getProviderLinks(provider), expectedLinks);
  NewTabUtils.links.removeProvider(provider);
});

add_task(function* notifyLinkDelete() {
  let expectedLinks = makeLinks(0, 3, 1);

  let provider = new TestProvider(done => done(expectedLinks));
  provider.maxNumLinks = expectedLinks.length;

  NewTabUtils.initWithoutProviders();
  NewTabUtils.links.addProvider(provider);
  yield new Promise(resolve => NewTabUtils.links.populateCache(resolve));

  do_check_links(NewTabUtils.links.getLinks(), expectedLinks);

  // Remove a link.
  let removedLink = expectedLinks[2];
  provider.notifyLinkChanged(removedLink, 2, true);
  let links = NewTabUtils.links._providers.get(provider);

  // Check that sortedLinks is correctly updated.
  do_check_links(NewTabUtils.links.getLinks(), expectedLinks.slice(0, 2));

  // Check that linkMap is accurately updated.
  do_check_eq(links.linkMap.size, 2);
  do_check_true(links.linkMap.get(expectedLinks[0].url));
  do_check_true(links.linkMap.get(expectedLinks[1].url));
  do_check_false(links.linkMap.get(removedLink.url));

  // Check that siteMap is correctly updated.
  do_check_eq(links.siteMap.size, 2);
  do_check_true(links.siteMap.has(NewTabUtils.extractSite(expectedLinks[0].url)));
  do_check_true(links.siteMap.has(NewTabUtils.extractSite(expectedLinks[1].url)));
  do_check_false(links.siteMap.has(NewTabUtils.extractSite(removedLink.url)));

  NewTabUtils.links.removeProvider(provider);
});

add_task(function* populatePromise() {
  let count = 0;
  let expectedLinks = makeLinks(0, 10, 2);

  let getLinksFcn = Task.async(function* (callback) {
    //Should not be calling getLinksFcn twice
    count++;
    do_check_eq(count, 1);
    yield Promise.resolve();
    callback(expectedLinks);
  });

  let provider = new TestProvider(getLinksFcn);

  NewTabUtils.initWithoutProviders();
  NewTabUtils.links.addProvider(provider);

  NewTabUtils.links.populateProviderCache(provider, () => {});
  NewTabUtils.links.populateProviderCache(provider, () => {
    do_check_links(NewTabUtils.links.getLinks(), expectedLinks);
    NewTabUtils.links.removeProvider(provider);
  });
});

add_task(function* isTopSiteGivenProvider() {
  let expectedLinks = makeLinks(0, 10, 2);

  // The lowest 2 frecencies have the same base domain.
  expectedLinks[expectedLinks.length - 2].url = expectedLinks[expectedLinks.length - 1].url + "Test";

  let provider = new TestProvider(done => done(expectedLinks));
  provider.maxNumLinks = expectedLinks.length;

  NewTabUtils.initWithoutProviders();
  NewTabUtils.links.addProvider(provider);
  yield new Promise(resolve => NewTabUtils.links.populateCache(resolve));

  do_check_eq(NewTabUtils.isTopSiteGivenProvider("example2.com", provider), true);
  do_check_eq(NewTabUtils.isTopSiteGivenProvider("example1.com", provider), false);

  // Push out frecency 2 because the maxNumLinks is reached when adding frecency 3
  let newLink = makeLink(3);
  provider.notifyLinkChanged(newLink);

  // There is still a frecent url with example2 domain, so it's still frecent.
  do_check_eq(NewTabUtils.isTopSiteGivenProvider("example3.com", provider), true);
  do_check_eq(NewTabUtils.isTopSiteGivenProvider("example2.com", provider), true);

  // Push out frecency 3
  newLink = makeLink(5);
  provider.notifyLinkChanged(newLink);

  // Push out frecency 4
  newLink = makeLink(9);
  provider.notifyLinkChanged(newLink);

  // Our count reached 0 for the example2.com domain so it's no longer a frecent site.
  do_check_eq(NewTabUtils.isTopSiteGivenProvider("example5.com", provider), true);
  do_check_eq(NewTabUtils.isTopSiteGivenProvider("example2.com", provider), false);

  NewTabUtils.links.removeProvider(provider);
});

add_task(function* multipleProviders() {
  // Make each provider generate NewTabUtils.links.maxNumLinks links to check
  // that no more than maxNumLinks are actually returned in the merged list.
  let evenLinks = makeLinks(0, 2 * NewTabUtils.links.maxNumLinks, 2);
  let evenProvider = new TestProvider(done => done(evenLinks));
  let oddLinks = makeLinks(0, 2 * NewTabUtils.links.maxNumLinks - 1, 2);
  let oddProvider = new TestProvider(done => done(oddLinks));

  NewTabUtils.initWithoutProviders();
  NewTabUtils.links.addProvider(evenProvider);
  NewTabUtils.links.addProvider(oddProvider);

  yield new Promise(resolve => NewTabUtils.links.populateCache(resolve));

  let links = NewTabUtils.links.getLinks();
  let expectedLinks = makeLinks(NewTabUtils.links.maxNumLinks,
                                2 * NewTabUtils.links.maxNumLinks,
                                1);
  do_check_eq(links.length, NewTabUtils.links.maxNumLinks);
  do_check_links(links, expectedLinks);

  NewTabUtils.links.removeProvider(evenProvider);
  NewTabUtils.links.removeProvider(oddProvider);
});

add_task(function* changeLinks() {
  let expectedLinks = makeLinks(0, 20, 2);
  let provider = new TestProvider(done => done(expectedLinks));

  NewTabUtils.initWithoutProviders();
  NewTabUtils.links.addProvider(provider);

  yield new Promise(resolve => NewTabUtils.links.populateCache(resolve));

  do_check_links(NewTabUtils.links.getLinks(), expectedLinks);

  // Notify of a new link.
  let newLink = makeLink(19);
  expectedLinks.splice(1, 0, newLink);
  provider.notifyLinkChanged(newLink);
  do_check_links(NewTabUtils.links.getLinks(), expectedLinks);

  // Notify of a link that's changed sort criteria.
  newLink.frecency = 17;
  expectedLinks.splice(1, 1);
  expectedLinks.splice(2, 0, newLink);
  provider.notifyLinkChanged({
    url: newLink.url,
    frecency: 17,
  });
  do_check_links(NewTabUtils.links.getLinks(), expectedLinks);

  // Notify of a link that's changed title.
  newLink.title = "My frecency is now 17";
  provider.notifyLinkChanged({
    url: newLink.url,
    title: newLink.title,
  });
  do_check_links(NewTabUtils.links.getLinks(), expectedLinks);

  // Notify of a new link again, but this time make it overflow maxNumLinks.
  provider.maxNumLinks = expectedLinks.length;
  newLink = makeLink(21);
  expectedLinks.unshift(newLink);
  expectedLinks.pop();
  do_check_eq(expectedLinks.length, provider.maxNumLinks); // Sanity check.
  provider.notifyLinkChanged(newLink);
  do_check_links(NewTabUtils.links.getLinks(), expectedLinks);

  // Notify of many links changed.
  expectedLinks = makeLinks(0, 3, 1);
  provider.notifyManyLinksChanged();

  // Since _populateProviderCache() is async, we must wait until the provider's
  // populate promise has been resolved.
  yield NewTabUtils.links._providers.get(provider).populatePromise;

  // NewTabUtils.links will now repopulate its cache
  do_check_links(NewTabUtils.links.getLinks(), expectedLinks);

  NewTabUtils.links.removeProvider(provider);
});

add_task(function* oneProviderAlreadyCached() {
  let links1 = makeLinks(0, 10, 1);
  let provider1 = new TestProvider(done => done(links1));

  NewTabUtils.initWithoutProviders();
  NewTabUtils.links.addProvider(provider1);

  yield new Promise(resolve => NewTabUtils.links.populateCache(resolve));
  do_check_links(NewTabUtils.links.getLinks(), links1);

  let links2 = makeLinks(10, 20, 1);
  let provider2 = new TestProvider(done => done(links2));
  NewTabUtils.links.addProvider(provider2);

  yield new Promise(resolve => NewTabUtils.links.populateCache(resolve));
  do_check_links(NewTabUtils.links.getLinks(), links2.concat(links1));

  NewTabUtils.links.removeProvider(provider1);
  NewTabUtils.links.removeProvider(provider2);
});

add_task(function* newLowRankedLink() {
  // Init a provider with 10 links and make its maximum number also 10.
  let links = makeLinks(0, 10, 1);
  let provider = new TestProvider(done => done(links));
  provider.maxNumLinks = links.length;

  NewTabUtils.initWithoutProviders();
  NewTabUtils.links.addProvider(provider);

  yield new Promise(resolve => NewTabUtils.links.populateCache(resolve));
  do_check_links(NewTabUtils.links.getLinks(), links);

  // Notify of a new link that's low-ranked enough not to make the list.
  let newLink = makeLink(0);
  provider.notifyLinkChanged(newLink);
  do_check_links(NewTabUtils.links.getLinks(), links);

  // Notify about the new link's title change.
  provider.notifyLinkChanged({
    url: newLink.url,
    title: "a new title",
  });
  do_check_links(NewTabUtils.links.getLinks(), links);

  NewTabUtils.links.removeProvider(provider);
});

add_task(function* extractSite() {
  // All these should extract to the same site
  [ "mozilla.org",
    "m.mozilla.org",
    "mobile.mozilla.org",
    "www.mozilla.org",
    "www3.mozilla.org",
  ].forEach(host => {
    let url = "http://" + host;
    do_check_eq(NewTabUtils.extractSite(url), "mozilla.org", "extracted same " + host);
  });

  // All these should extract to the same subdomain
  [ "bugzilla.mozilla.org",
    "www.bugzilla.mozilla.org",
  ].forEach(host => {
    let url = "http://" + host;
    do_check_eq(NewTabUtils.extractSite(url), "bugzilla.mozilla.org", "extracted eTLD+2 " + host);
  });

  // All these should not extract to the same site
  [ "bugzilla.mozilla.org",
    "bug123.bugzilla.mozilla.org",
    "too.many.levels.bugzilla.mozilla.org",
    "m2.mozilla.org",
    "mobile30.mozilla.org",
    "ww.mozilla.org",
    "ww2.mozilla.org",
    "wwwww.mozilla.org",
    "wwwww50.mozilla.org",
    "wwws.mozilla.org",
    "secure.mozilla.org",
    "secure10.mozilla.org",
    "many.levels.deep.mozilla.org",
    "just.check.in",
    "192.168.0.1",
    "localhost",
  ].forEach(host => {
    let url = "http://" + host;
    do_check_neq(NewTabUtils.extractSite(url), "mozilla.org", "extracted diff " + host);
  });

  // All these should not extract to the same site
  [ "about:blank",
    "file:///Users/user/file",
    "chrome://browser/something",
    "ftp://ftp.mozilla.org/",
  ].forEach(url => {
    do_check_neq(NewTabUtils.extractSite(url), "mozilla.org", "extracted diff url " + url);
  });
});

function TestProvider(getLinksFn) {
  this.getLinks = getLinksFn;
  this._observers = new Set();
}

TestProvider.prototype = {
  addObserver: function (observer) {
    this._observers.add(observer);
  },
  notifyLinkChanged: function (link, index=-1, deleted=false) {
    this._notifyObservers("onLinkChanged", link, index, deleted);
  },
  notifyManyLinksChanged: function () {
    this._notifyObservers("onManyLinksChanged");
  },
  _notifyObservers: function () {
    let observerMethodName = arguments[0];
    let args = Array.prototype.slice.call(arguments, 1);
    args.unshift(this);
    for (let obs of this._observers) {
      if (obs[observerMethodName])
        obs[observerMethodName].apply(NewTabUtils.links, args);
    }
  },
};

function do_check_links(actualLinks, expectedLinks) {
  do_check_true(Array.isArray(actualLinks));
  do_check_eq(actualLinks.length, expectedLinks.length);
  for (let i = 0; i < expectedLinks.length; i++) {
    let expected = expectedLinks[i];
    let actual = actualLinks[i];
    do_check_eq(actual.url, expected.url);
    do_check_eq(actual.title, expected.title);
    do_check_eq(actual.frecency, expected.frecency);
    do_check_eq(actual.lastVisitDate, expected.lastVisitDate);
  }
}

function makeLinks(frecRangeStart, frecRangeEnd, step) {
  let links = [];
  // Remember, links are ordered by frecency descending.
  for (let i = frecRangeEnd; i > frecRangeStart; i -= step) {
    links.push(makeLink(i));
  }
  return links;
}

function makeLink(frecency) {
  return {
    url: "http://example" + frecency + ".com/",
    title: "My frecency is " + frecency,
    frecency: frecency,
    lastVisitDate: 0,
  };
}
