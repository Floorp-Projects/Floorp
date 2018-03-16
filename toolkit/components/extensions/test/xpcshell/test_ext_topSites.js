"use strict";

ChromeUtils.import("resource://gre/modules/PlacesUtils.jsm");
ChromeUtils.import("resource://gre/modules/NewTabUtils.jsm");

add_task(async function test_topSites() {
  let visits = [];
  const numVisits = 15; // To make sure we get frecency.
  let visitDate = new Date(1999, 9, 9, 9, 9).getTime();

  // Stick a couple sites into history.
  for (let i = 0; i < 2; ++i) {
    let visit = {
      url: `http://example.com${i}/`,
      title: `visit${i}`,
      visits: [],
    };
    for (let j = 0; j < numVisits; ++j) {
      visitDate -= 1000;
      visit.visits.push({date: new Date(visitDate)});
    }
    visits.push(visit);
  }
  NewTabUtils.init();

  await PlacesUtils.history.insertMany(visits);

  // Ensure our links show up in activityStream.
  let links = await NewTabUtils.activityStreamLinks.getTopSites();
  equal(links.length, visits.length, "Top sites has been successfully initialized");

  // Drop the visits.visits for later testing.
  visits = visits.map(v => { return {url: v.url, title: v.title}; });

  // Test that results from all providers are returned by default.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": [
        "topSites",
      ],
    },
    background() {
      // Tests consistent behaviour when no providers are specified.
      browser.test.onMessage.addListener(async providers => {
        let sites;
        if (typeof providers !== undefined) {
          sites = await browser.topSites.get(providers);
        } else {
          sites = await browser.topSites.get();
        }
        browser.test.sendMessage("sites", sites);
      });
    },
  });

  await extension.startup();

  function getSites(providers) {
    extension.sendMessage(providers);
    return extension.awaitMessage("sites");
  }

  Assert.deepEqual(visits, await getSites(), "got topSites");
  Assert.deepEqual(visits, await getSites({}), "got topSites");
  Assert.deepEqual(visits, await getSites({providers: ["places"]}), "got topSites");
  Assert.deepEqual(visits, await getSites({providers: ["activityStream"]}), "got topSites");
  Assert.deepEqual(visits, await getSites({providers: ["places", "activityStream"]}), "got topSites");

  NewTabUtils.uninit();
  await extension.unload();
  await PlacesUtils.history.clear();
});
