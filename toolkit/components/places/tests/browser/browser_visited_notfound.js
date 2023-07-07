/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test() {
  const url = "http://mochi.test:8888/notFoundPage.html";

  // Ensure that decay frecency doesn't kick in during tests (as a result
  // of idle-daily).
  await SpecialPowers.pushPrefEnv({
    set: [["places.frecency.decayRate", "1.0"]],
  });
  await registerCleanupFunction(PlacesUtils.history.clear);

  // First add a visit to the page, this will ensure that later we skip
  // updating the frecency for a newly not-found page.
  await PlacesTestUtils.addVisits(url);
  let frecency = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "frecency",
    { url }
  );
  Assert.equal(frecency, 100, "Check initial frecency");

  // Used to verify errors are not marked as typed.
  PlacesUtils.history.markPageAsTyped(NetUtil.newURI(url));

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async browser => {
      info("awaiting for the visit");

      Assert.equal(
        await PlacesTestUtils.getDatabaseValue("moz_places", "frecency", {
          url,
        }),
        frecency,
        "Frecency should be unchanged"
      );
      Assert.equal(
        await PlacesTestUtils.getDatabaseValue("moz_places", "hidden", {
          url,
        }),
        0,
        "Page should not be hidden"
      );
      Assert.equal(
        await PlacesTestUtils.getDatabaseValue("moz_places", "typed", {
          url,
        }),
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
    }
  );
});
