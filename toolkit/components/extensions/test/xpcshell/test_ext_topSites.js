"use strict";

ChromeUtils.import("resource://gre/modules/PlacesUtils.jsm");
ChromeUtils.import("resource://gre/modules/NewTabUtils.jsm");
ChromeUtils.import("resource://testing-common/PlacesTestUtils.jsm");

// A small 1x1 test png
const IMAGE_1x1 = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAAAAAA6fptVAAAACklEQVQI12NgAAAAAgAB4iG8MwAAAABJRU5ErkJggg==";

add_task(async function test_topSites() {
  let visits = [];
  const numVisits = 15; // To make sure we get frecency.
  let visitDate = new Date(1999, 9, 9, 9, 9).getTime();

  function setVisit(visit) {
    for (let j = 0; j < numVisits; ++j) {
      visitDate -= 1000;
      visit.visits.push({date: new Date(visitDate)});
    }
    visits.push(visit);
  }
  // Stick a couple sites into history.
  for (let i = 0; i < 2; ++i) {
    setVisit({
      url: `http://example${i}.com/`,
      title: `visit${i}`,
      visits: [],
    });
    setVisit({
      url: `http://www.example${i}.com/foobar`,
      title: `visit${i}-www`,
      visits: [],
    });
  }
  NewTabUtils.init();
  await PlacesUtils.history.insertMany(visits);

  // Insert a favicon to show that favicons are not returned by default.
  let faviconData = new Map();
  faviconData.set("http://example0.com", IMAGE_1x1);
  await PlacesTestUtils.addFavicons(faviconData);

  // Ensure our links show up in activityStream.
  let links = await NewTabUtils.activityStreamLinks.getTopSites({onePerDomain: false, topsiteFrecency: 1});

  equal(links.length, visits.length, "Top sites has been successfully initialized");

  // Drop the visits.visits for later testing.
  visits = visits.map(v => { return {url: v.url, title: v.title, favicon: undefined}; });

  // Test that results from all providers are returned by default.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": [
        "topSites",
      ],
    },
    background() {
      browser.test.onMessage.addListener(async options => {
        let sites;
        if (typeof options !== undefined) {
          sites = await browser.topSites.get(options);
        } else {
          sites = await browser.topSites.get();
        }
        browser.test.sendMessage("sites", sites);
      });
    },
  });

  await extension.startup();

  function getSites(options) {
    extension.sendMessage(options);
    return extension.awaitMessage("sites");
  }

  Assert.deepEqual([visits[0], visits[2]],
                   await getSites(),
                   "got topSites default");
  Assert.deepEqual(visits,
                   await getSites({onePerDomain: false}),
                   "got topSites all links");

  NewTabUtils.activityStreamLinks.blockURL(visits[0]);
  ok(NewTabUtils.blockedLinks.isBlocked(visits[0]), `link ${visits[0].url} is blocked`);

  Assert.deepEqual([visits[2], visits[1]],
                   await getSites(),
                   "got topSites with blocked links filtered out");
  Assert.deepEqual([visits[0], visits[2]],
                   await getSites({includeBlocked: true}),
                   "got topSites with blocked links included");

  // Test favicon result
  let topSites = await getSites({includeBlocked: true, includeFavicon: true});
  equal(topSites[0].favicon, IMAGE_1x1, "received favicon");

  equal(1, (await getSites({limit: 1, includeBlocked: true})).length, "limit 1 topSite");

  NewTabUtils.uninit();
  await extension.unload();
  await PlacesUtils.history.clear();
});
