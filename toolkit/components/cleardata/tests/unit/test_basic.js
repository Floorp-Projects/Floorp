/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Basic test for nsIClearDataService module.
 */

"use strict";

add_task(async function test_basic() {
  Assert.ok(!!Services.clearData);

  await new Promise(aResolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => {
      Assert.equal(value, 0);
      aResolve();
    });
  });
});
