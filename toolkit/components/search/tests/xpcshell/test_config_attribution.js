/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function () {
  Region._setHomeRegion("an", false);
  await SearchTestUtils.useTestEngines("test-extensions");
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

add_task(async function test_send_attribution_request() {
  let engine = await Services.search.getEngineByName("Plain");
  Assert.ok(
    engine.sendAttributionRequest,
    "Should have noted to send the attribution request for Plain"
  );

  engine = await Services.search.getEngineByName("Special");
  Assert.ok(
    engine.sendAttributionRequest,
    "Should have noted to send the attribution request for Special"
  );

  engine = await Services.search.getEngineByName("Multilocale AN");
  Assert.ok(
    !engine.sendAttributionRequest,
    "Should not have noted to send the attribution request for Multilocale AN"
  );
});
