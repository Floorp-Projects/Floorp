/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/Services.jsm");

function run_test() {
  test_methods_presence();
  test_methods_calling();

  ok(true);
}

function test_methods_presence() {
  equal(Services.intl.getCalendarInfo instanceof Function, true);
  equal(Services.intl.getDisplayNames instanceof Function, true);
  equal(Services.intl.getLocaleInfo instanceof Function, true);
  equal(Services.intl.createDateTimeFormat instanceof Function, true);
}

function test_methods_calling() {
  Services.intl.getCalendarInfo("pl");
  Services.intl.getDisplayNames("ar");
  Services.intl.getLocaleInfo("de");
  Services.intl.createDateTimeFormat("fr");
  ok(true);
}
