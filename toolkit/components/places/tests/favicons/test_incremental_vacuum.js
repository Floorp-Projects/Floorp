/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests incremental vacuum of the favicons database.

const { PlacesDBUtils } = ChromeUtils.import(
  "resource://gre/modules/PlacesDBUtils.jsm"
);

add_task(async function() {
  let icon = {
    file: do_get_file("noise.png"),
    mimetype: "image/png",
  };

  let url = "http://foo.bar/";
  await PlacesTestUtils.addVisits(url);
  for (let i = 0; i < 10; ++i) {
    let iconUri = NetUtil.newURI("http://mozilla.org/" + i);
    let data = readFileData(icon.file);
    PlacesUtils.favicons.replaceFaviconData(iconUri, data, icon.mimetype);
    await setFaviconForPage(url, iconUri);
  }

  let promise = TestUtils.topicObserved("places-favicons-expired");
  PlacesUtils.favicons.expireAllFavicons();
  await promise;

  let db = await PlacesUtils.promiseDBConnection();
  let state = (
    await db.execute("PRAGMA favicons.auto_vacuum")
  )[0].getResultByIndex(0);
  Assert.equal(state, 2, "auto_vacuum should be incremental");
  let count = (
    await db.execute("PRAGMA favicons.freelist_count")
  )[0].getResultByIndex(0);
  info(`Found ${count} freelist pages`);
  let log = await PlacesDBUtils.incrementalVacuum();
  info(log);
  let newCount = (
    await db.execute("PRAGMA favicons.freelist_count")
  )[0].getResultByIndex(0);
  info(`Found ${newCount} freelist pages`);
  Assert.ok(
    newCount < count,
    "The number of freelist pages should have reduced"
  );
});
