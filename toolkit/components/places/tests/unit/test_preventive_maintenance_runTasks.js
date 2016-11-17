/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

 /**
  * Test preventive maintenance runTasks.
  */

// Include PlacesDBUtils module.
Components.utils.import("resource://gre/modules/PlacesDBUtils.jsm");

function run_test() {
  do_test_pending();
  PlacesDBUtils.runTasks([PlacesDBUtils.reindex], function(aLog) {
    let sections = [];
    let positives = [];
    let negatives = [];
    let infos = [];

    aLog.forEach(function(aMsg) {
      print(aMsg);
      switch (aMsg.substr(0, 1)) {
        case "+":
          positives.push(aMsg);
          break;
        case "-":
          negatives.push(aMsg);
          break;
        case ">":
          sections.push(aMsg);
          break;
        default:
          infos.push(aMsg);
      }
    });

    print("Check that we have run all sections.");
    do_check_eq(sections.length, 1);
    print("Check that we have no negatives.");
    do_check_false(!!negatives.length);
    print("Check that we have positives.");
    do_check_true(!!positives.length);

    do_test_finished();
  });
}
