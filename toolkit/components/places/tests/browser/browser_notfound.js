/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function () {
  const url = "http://mochi.test:8888/notFoundPage.html";

  await registerCleanupFunction(PlacesUtils.history.clear);

  // Used to verify errors are not marked as typed.
  PlacesUtils.history.markPageAsTyped(NetUtil.newURI(url));

  let promiseVisited = PlacesTestUtils.waitForNotification(
    "page-visited",
    events => {
      console.log(JSON.stringify(events));
      return events.length == 1 && events[0].url === url;
    }
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async browser => {
      info("awaiting for the visit");
      await promiseVisited;

      Assert.equal(
        await PlacesTestUtils.getDatabaseValue("moz_places", "frecency", {
          url,
        }),
        0,
        "Frecency should be 0"
      );
      Assert.equal(
        await PlacesTestUtils.getDatabaseValue("moz_places", "hidden", { url }),
        0,
        "Page should not be hidden"
      );
      Assert.equal(
        await PlacesTestUtils.getDatabaseValue("moz_places", "typed", { url }),
        0,
        "page should not be marked as typed"
      );
      Assert.equal(
        await PlacesTestUtils.getDatabaseValue(
          "moz_places",
          "recalc_frecency",
          { url }
        ),
        0,
        "page should not be marked for frecency recalculation"
      );
      Assert.equal(
        await PlacesTestUtils.getDatabaseValue(
          "moz_places",
          "recalc_alt_frecency",
          { url }
        ),
        0,
        "page should not be marked for alt frecency recalculation"
      );

      info("Adding new valid visits should cause recalculation");
      await PlacesTestUtils.addVisits([url, "https://othersite.org/"]);
      let frecency = await PlacesTestUtils.getDatabaseValue(
        "moz_places",
        "frecency",
        { url }
      );
      Assert.greater(frecency, 0, "Check frecency was updated");
    }
  );
});
