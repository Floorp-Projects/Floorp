/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  const PROPERTIES = ["name", "host", "arch", "version", "pagesize",
                      "pageshift", "memmapalign", "cpucount", "memsize"];
  let sysInfo = Components.classes["@mozilla.org/system-info;1"].
                getService(Components.interfaces.nsIPropertyBag2);

  PROPERTIES.forEach(function(aPropertyName) {
    print("Testing property: " + aPropertyName);
    let value = sysInfo.getProperty(aPropertyName);
    do_check_true(!!value);
  });

  // This property must exist, but its value might be zero.
  print("Testing property: umask")
  do_check_eq(typeof sysInfo.getProperty("umask"), "number");
}
