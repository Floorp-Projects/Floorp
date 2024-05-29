/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test to ensure nsIClearDataService module flags are different.
 */

"use strict";

function checkIfOneBit(bits) {
  return bits && !(bits & (bits - 1));
}

add_task(function test_clear_data_service_flags() {
  let prevAllFlags = 0;
  let allFlags = 0;
  Object.keys(Ci.nsIClearDataService)
    .filter(k => k.startsWith("CLEAR_"))
    .forEach(flag => {
      // checks that all one bit flags are different
      const FLAG_VAL = Services.clearData[flag];

      if (checkIfOneBit(FLAG_VAL)) {
        prevAllFlags = allFlags;
        allFlags |= FLAG_VAL;
        // if allFlags didn't change, this flag was same as another flag
        Assert.notEqual(
          allFlags,
          prevAllFlags,
          `Value of ${flag} should be unique.`
        );
      }
    });
});
