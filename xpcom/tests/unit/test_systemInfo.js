/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  const PROPERTIES = [
    "name",
    "arch",
    "version",
    "pagesize",
    "pageshift",
    "memmapalign",
    "memsize",
  ];
  let sysInfo = Services.sysinfo;

  PROPERTIES.forEach(function(aPropertyName) {
    print("Testing property: " + aPropertyName);
    let value = sysInfo.getProperty(aPropertyName);
    Assert.ok(!!value);
  });

  // This property must exist, but its value might be zero.
  print("Testing property: umask");
  Assert.equal(typeof sysInfo.getProperty("umask"), "number");
}
