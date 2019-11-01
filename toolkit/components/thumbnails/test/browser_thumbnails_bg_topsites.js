/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const image1x1 =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAAAAAA6fptVAAAACklEQVQI12NgAAAAAgAB4iG8MwAAAABJRU5ErkJggg==";
const image96x96 =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAGAAAAAJCAYAAADNYymqAAAAKklEQVR42u3RMQEAAAjDMFCO9CGDg1RC00lN6awGAACADQAACAAAAXjXApPGFm+IdJG9AAAAAElFTkSuQmCC";
const baseURL = "http://mozilla${i}.com/";

add_task(async function thumbnails_bg_topsites() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.improvesearch.topSiteSearchShortcuts",
        false,
      ],
    ],
  });
  // Add 3 top sites - 2 visits each so it can pass frecency threshold of the top sites query
  for (let i = 1; i <= 3; i++) {
    await PlacesTestUtils.addVisits(baseURL.replace("${i}", i));
    await PlacesTestUtils.addVisits(baseURL.replace("${i}", i));
  }

  // Add favicon data for 2 of the top sites
  let faviconData = new Map();
  faviconData.set("http://mozilla1.com/", image1x1);
  faviconData.set("http://mozilla2.com/", image96x96);
  await PlacesTestUtils.addFavicons(faviconData);

  // Sanity check that we've successfully added all 3 urls to top sites
  let links = await NewTabUtils.activityStreamLinks.getTopSites();
  is(
    links[0].url,
    baseURL.replace("${i}", 3),
    "Top site has been successfully added"
  );
  is(
    links[1].url,
    baseURL.replace("${i}", 2),
    "Top site has been successfully added"
  );
  is(
    links[2].url,
    baseURL.replace("${i}", 1),
    "Top site has been successfully added"
  );

  // Now, add a pinned site so we can also fetch a screenshot for that
  const pinnedSite = { url: baseURL.replace("${i}", 4) };
  NewTabUtils.pinnedLinks.pin(pinnedSite, 0);

  // Check that the correct sites will capture screenshots
  gBrowserThumbnails.clearTopSiteURLCache();
  let topSites = await gBrowserThumbnails._topSiteURLs;
  ok(
    topSites.includes("http://mozilla1.com/"),
    "Top site did not have a rich icon - get a screenshot"
  );
  ok(
    topSites.includes("http://mozilla3.com/"),
    "Top site did not have an icon - get a screenshot"
  );
  ok(
    topSites.includes("http://mozilla4.com/"),
    "Site is pinned - get a screenshot"
  );
  ok(
    !topSites.includes("http://mozilla2.com/"),
    "Top site had a rich icon - do not get a screenshot"
  );

  // Clean up
  NewTabUtils.pinnedLinks.unpin(pinnedSite);
  await PlacesUtils.history.clear();
});
