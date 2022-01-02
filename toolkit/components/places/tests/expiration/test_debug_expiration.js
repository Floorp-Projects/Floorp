/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * What this is aimed to test:
 *
 * Expiration can be manually triggered through a debug topic, but that should
 * only expire orphan entries, unless -1 is passed as limit.
 */

var gNow = getExpirablePRTime(60);

add_task(async function test_expire_orphans() {
  // Add visits to 2 pages and force a orphan expiration. Visits should survive.
  await PlacesTestUtils.addVisits({
    uri: uri("http://page1.mozilla.org/"),
    visitDate: gNow++,
  });
  await PlacesTestUtils.addVisits({
    uri: uri("http://page2.mozilla.org/"),
    visitDate: gNow++,
  });
  // Create a orphan place.
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://page3.mozilla.org/",
    title: "",
  });
  await PlacesUtils.bookmarks.remove(bm);

  // Expire now.
  await promiseForceExpirationStep(0);

  // Check that visits survived.
  Assert.equal(visits_in_database("http://page1.mozilla.org/"), 1);
  Assert.equal(visits_in_database("http://page2.mozilla.org/"), 1);
  Assert.ok(!page_in_database("http://page3.mozilla.org/"));

  // Clean up.
  await PlacesUtils.history.clear();
});

add_task(async function test_expire_orphans_optionalarg() {
  // Add visits to 2 pages and force a orphan expiration. Visits should survive.
  await PlacesTestUtils.addVisits({
    uri: uri("http://page1.mozilla.org/"),
    visitDate: gNow++,
  });
  await PlacesTestUtils.addVisits({
    uri: uri("http://page2.mozilla.org/"),
    visitDate: gNow++,
  });
  // Create a orphan place.
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://page3.mozilla.org/",
    title: "",
  });
  await PlacesUtils.bookmarks.remove(bm);

  // Expire now.
  await promiseForceExpirationStep();

  // Check that visits survived.
  Assert.equal(visits_in_database("http://page1.mozilla.org/"), 1);
  Assert.equal(visits_in_database("http://page2.mozilla.org/"), 1);
  Assert.ok(!page_in_database("http://page3.mozilla.org/"));

  // Clean up.
  await PlacesUtils.history.clear();
});

add_task(async function test_expire_limited() {
  await PlacesTestUtils.addVisits([
    {
      // Should be expired cause it's the oldest visit
      uri: "http://old.mozilla.org/",
      visitDate: gNow++,
    },
    {
      // Should not be expired cause we limit 1
      uri: "http://new.mozilla.org/",
      visitDate: gNow++,
    },
  ]);

  // Expire now.
  await promiseForceExpirationStep(1);

  // Check that newer visit survived.
  Assert.equal(visits_in_database("http://new.mozilla.org/"), 1);
  // Other visits should have been expired.
  Assert.ok(!page_in_database("http://old.mozilla.org/"));

  // Clean up.
  await PlacesUtils.history.clear();
});

add_task(async function test_expire_limited_longurl() {
  let longurl = "http://long.mozilla.org/" + "a".repeat(232);
  await PlacesTestUtils.addVisits([
    {
      // Should be expired cause it's the oldest visit
      uri: "http://old.mozilla.org/",
      visitDate: gNow++,
    },
    {
      // Should be expired cause it's a long url older than 60 days.
      uri: longurl,
      visitDate: gNow++,
    },
    {
      // Should not be expired cause younger than 60 days.
      uri: longurl,
      visitDate: getExpirablePRTime(58),
    },
  ]);

  await promiseForceExpirationStep(1);

  // Check that some visits survived.
  Assert.equal(visits_in_database(longurl), 1);
  // Other visits should have been expired.
  Assert.ok(!page_in_database("http://old.mozilla.org/"));

  // Clean up.
  await PlacesUtils.history.clear();
});

add_task(async function test_expire_limited_exoticurl() {
  await PlacesTestUtils.addVisits([
    {
      // Should be expired cause it's the oldest visit
      uri: "http://old.mozilla.org/",
      visitDate: gNow++,
    },
    {
      // Should be expired cause it's a long url older than 60 days.
      uri: "http://download.mozilla.org",
      visitDate: gNow++,
      transition: 7,
    },
    {
      // Should not be expired cause younger than 60 days.
      uri: "http://nonexpirable-download.mozilla.org",
      visitDate: getExpirablePRTime(58),
      transition: 7,
    },
  ]);

  await promiseForceExpirationStep(1);

  // Check that some visits survived.
  Assert.equal(
    visits_in_database("http://nonexpirable-download.mozilla.org/"),
    1
  );
  // The visits are gone, the url is not yet, cause we limited the expiration
  // to one entry, and we already removed http://old.mozilla.org/.
  // The page normally would be expired by the next expiration run.
  Assert.equal(visits_in_database("http://download.mozilla.org/"), 0);
  // Other visits should have been expired.
  Assert.ok(!page_in_database("http://old.mozilla.org/"));

  // Clean up.
  await PlacesUtils.history.clear();
});

add_task(async function test_expire_unlimited() {
  let longurl = "http://long.mozilla.org/" + "a".repeat(232);
  await PlacesTestUtils.addVisits([
    {
      uri: "http://old.mozilla.org/",
      visitDate: gNow++,
    },
    {
      uri: "http://new.mozilla.org/",
      visitDate: gNow++,
    },
    // Add expirable visits.
    {
      uri: "http://download.mozilla.org/",
      visitDate: gNow++,
      transition: PlacesUtils.history.TRANSITION_DOWNLOAD,
    },
    {
      uri: longurl,
      visitDate: gNow++,
    },

    // Add non-expirable visits
    {
      uri: "http://nonexpirable.mozilla.org/",
      visitDate: getExpirablePRTime(5),
    },
    {
      uri: "http://nonexpirable-download.mozilla.org/",
      visitDate: getExpirablePRTime(5),
      transition: PlacesUtils.history.TRANSITION_DOWNLOAD,
    },
    {
      uri: longurl,
      visitDate: getExpirablePRTime(5),
    },
  ]);

  await promiseForceExpirationStep(-1);

  // Check that some visits survived.
  Assert.equal(visits_in_database("http://nonexpirable.mozilla.org/"), 1);
  Assert.equal(
    visits_in_database("http://nonexpirable-download.mozilla.org/"),
    1
  );
  Assert.equal(visits_in_database(longurl), 1);
  // Other visits should have been expired.
  Assert.ok(!page_in_database("http://old.mozilla.org/"));
  Assert.ok(!page_in_database("http://download.mozilla.org/"));
  Assert.ok(!page_in_database("http://new.mozilla.org/"));

  // Clean up.
  await PlacesUtils.history.clear();
});

add_task(async function test_expire_icons() {
  const dataUrl =
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAA" +
    "AAAA6fptVAAAACklEQVQI12NgAAAAAgAB4iG8MwAAAABJRU5ErkJggg==";

  const entries = [
    {
      desc: "Expired because it redirects",
      page: "http://source.old.org/",
      icon: "http://source.old.org/test_icon.png",
      expired: true,
      redirect: "http://dest.old.org/",
      removed: true,
    },
    {
      desc: "Not expired because recent",
      page: "http://source.new.org/",
      icon: "http://source.new.org/test_icon.png",
      expired: false,
      redirect: "http://dest.new.org/",
      removed: false,
    },
    {
      desc: "Not expired because does not match, even if old",
      page: "http://stay.moz.org/",
      icon: "http://stay.moz.org/test_icon.png",
      expired: true,
      removed: false,
    },
    {
      desc: "Not expired because does not have a root icon, even if old",
      page: "http://noroot.ref.org/#test",
      icon: "http://noroot.ref.org/test_icon.png",
      expired: true,
      removed: false,
    },
    {
      desc: "Expired because has a root icon",
      page: "http://root.ref.org/#test",
      icon: "http://root.ref.org/test_icon.png",
      root: "http://root.ref.org/favicon.ico",
      expired: true,
      removed: true,
    },
    {
      desc: "Not expired because recent",
      page: "http://new.ref.org/#test",
      icon: "http://new.ref.org/test_icon.png",
      expired: false,
      root: "http://new.ref.org/favicon.ico",
      removed: false,
    },
  ];

  for (let entry of entries) {
    if (entry.redirect) {
      await PlacesTestUtils.addVisits(entry.page);
      await PlacesTestUtils.addVisits({
        uri: entry.redirect,
        transition: TRANSITION_REDIRECT_PERMANENT,
        referrer: entry.page,
      });
    } else {
      await PlacesTestUtils.addVisits(entry.page);
    }

    PlacesUtils.favicons.replaceFaviconDataFromDataURL(
      Services.io.newURI(entry.icon),
      dataUrl,
      0,
      Services.scriptSecurityManager.getSystemPrincipal()
    );
    await PlacesTestUtils.addFavicons(new Map([[entry.page, entry.icon]]));
    Assert.equal(
      await getFaviconUrlForPage(entry.page),
      entry.icon,
      "Sanity check the icon exists"
    );

    if (entry.root) {
      PlacesUtils.favicons.replaceFaviconDataFromDataURL(
        Services.io.newURI(entry.root),
        dataUrl,
        0,
        Services.scriptSecurityManager.getSystemPrincipal()
      );
      await PlacesTestUtils.addFavicons(new Map([[entry.page, entry.root]]));
    }
    if (entry.expired) {
      // Set an expired time on the icon.
      await PlacesUtils.withConnectionWrapper("expireFavicon", async db => {
        await db.execute(
          `UPDATE moz_icons SET expire_ms = 1 WHERE icon_url = :url`,
          { url: entry.icon }
        );
        if (entry.root) {
          await db.execute(
            `UPDATE moz_icons SET expire_ms = 1 WHERE icon_url = :url`,
            { url: entry.root }
          );
        }
      });
    }
  }

  info("Run expiration");
  await promiseForceExpirationStep(-1);

  info("Check expiration");
  // Remove the root icons before checking the associated icons have been expired.
  await PlacesUtils.withConnectionWrapper("test_debug_expiration.js", db =>
    db.execute(`DELETE FROM moz_icons WHERE root = 1`)
  );
  for (let entry of entries) {
    Assert.ok(page_in_database(entry.page));

    if (entry.removed) {
      await Assert.rejects(
        getFaviconUrlForPage(entry.page),
        /Unable to find an icon/,
        entry.desc
      );
    } else {
      Assert.equal(
        await getFaviconUrlForPage(entry.page),
        entry.icon,
        entry.desc
      );
    }
  }

  // Clean up.
  await PlacesUtils.history.clear();
});

function run_test() {
  // Set interval to a large value so we don't expire on it.
  setInterval(3600); // 1h
  // Set maxPages to a low value, so it's easy to go over it.
  setMaxPages(1);

  run_next_test();
}
