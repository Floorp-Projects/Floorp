/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Basic test for nsIClearDataService module.
 */

"use strict";

add_task(async function test_basic() {
  const service = Cc["@mozilla.org/clear-data-service;1"]
                  .getService(Ci.nsIClearDataService);
  Assert.ok(!!service);

  await new Promise(aResolve => {
    service.deleteData(Ci.nsIClearDataService.CLEAR_IMAGE_CACHE |
                       Ci.nsIClearDataService.CLEAR_COOKIES, value => {
      Assert.equal(value, 0);
      aResolve();
    });
  });
});
