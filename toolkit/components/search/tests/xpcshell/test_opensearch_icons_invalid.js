/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Test that an installed engine can't use a resource URL for an icon */

"use strict";

add_task(async function setup() {
  useHttpServer();
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_installedresourceicon() {
  let engine1 = await SearchTestUtils.promiseNewSearchEngine(
    `${gDataUrl}engine-resourceicon.xml`
  );
  let engine2 = await SearchTestUtils.promiseNewSearchEngine(
    `${gDataUrl}engine-chromeicon.xml`
  );

  Assert.equal(null, engine1.iconURI);
  Assert.equal(null, engine2.iconURI);
});
