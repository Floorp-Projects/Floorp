/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  const miHelper = Cc["@mozilla.org/mozintlhelper;1"].getService(
    Ci.mozIMozIntlHelper
  );

  test_this_global(miHelper);
  test_cross_global(miHelper);
  test_methods_presence(miHelper);

  ok(true);
}

function test_this_global(miHelper) {
  let x = {};

  miHelper.addGetCalendarInfo(x);
  equal(x.getCalendarInfo instanceof Function, true);
  equal(x.getCalendarInfo() instanceof Object, true);
}

function test_cross_global(miHelper) {
  var global = new Cu.Sandbox("https://example.com/");
  var x = global.Object();

  miHelper.addGetCalendarInfo(x);
  var waivedX = Cu.waiveXrays(x);
  equal(waivedX.getCalendarInfo instanceof Function, false);
  equal(
    waivedX.getCalendarInfo instanceof Cu.waiveXrays(global.Function),
    true
  );
  equal(waivedX.getCalendarInfo() instanceof Object, false);
  equal(
    waivedX.getCalendarInfo() instanceof Cu.waiveXrays(global.Object),
    true
  );
}

function test_methods_presence(miHelper) {
  equal(miHelper.addGetCalendarInfo instanceof Function, true);
  equal(miHelper.addGetDisplayNames instanceof Function, true);
  equal(miHelper.addGetLocaleInfo instanceof Function, true);
  equal(miHelper.addDateTimeFormatConstructor instanceof Function, true);

  let x = {};

  miHelper.addGetCalendarInfo(x);
  equal(x.getCalendarInfo instanceof Function, true);

  miHelper.addGetDisplayNames(x);
  equal(x.getDisplayNames instanceof Function, true);

  miHelper.addGetLocaleInfo(x);
  equal(x.getLocaleInfo instanceof Function, true);

  miHelper.addDateTimeFormatConstructor(x);
  equal(x.DateTimeFormat instanceof Function, true);
}
