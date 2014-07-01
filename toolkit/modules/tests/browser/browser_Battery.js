/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let imported = Components.utils.import("resource://gre/modules/Battery.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);

function test() {
  is(imported.Debugging.fake, false, "Battery spoofing is initially false")

  for (let k of ["charging", "chargingTime", "dischargingTime", "level"]) {
    Assert.throws(() => Battery[k] = 10, "Setting battery " + k + "preference without spoofing enabled should throw");
    ok(Battery[k] == Services.appShell.hiddenDOMWindow.navigator.battery[k], "Battery "+ k + " is correctly set");
  }
  imported.Debugging.fake = true;

  Battery.charging = true;
  Battery.chargingTime = 100;
  Battery.level = 0.5;
  ok(Battery.charging, "Test for charging setter");
  is(Battery.chargingTime, 100, "Test for chargingTime setter");
  is(Battery.level, 0.5, "Test for level setter");

  Battery.charging = false;
  Battery.dischargingTime = 50;
  Battery.level = 0.7;
  ok(!Battery.charging, "Test for charging setter");
  is(Battery.dischargingTime, 50, "Test for dischargingTime setter");
  is(Battery.level, 0.7, "Test for level setter");
}