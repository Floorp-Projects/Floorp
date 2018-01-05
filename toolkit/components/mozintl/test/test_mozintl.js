/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/Services.jsm");

function run_test() {
  test_methods_presence();
  test_methods_calling();
  test_constructors();

  ok(true);
}

function test_methods_presence() {
  equal(Services.intl.getCalendarInfo instanceof Function, true);
  equal(Services.intl.getDisplayNames instanceof Function, true);
  equal(Services.intl.getLocaleInfo instanceof Function, true);
  equal(Services.intl.getLocaleInfo instanceof Object, true);
}

function test_methods_calling() {
  Services.intl.getCalendarInfo("pl");
  Services.intl.getDisplayNames("ar");
  Services.intl.getLocaleInfo("de");
  new Services.intl.DateTimeFormat("fr");
  ok(true);
}

function test_constructors() {
  let dtf = new Intl.DateTimeFormat();
  let dtf2 = new Services.intl.DateTimeFormat();

  equal(typeof dtf, typeof dtf2);

  Assert.throws(() => {
    // This is an observable difference between Intl and mozIntl.
    //
    // Old ECMA402 APIs (edition 1 and 2) allowed for constructors to be called
    // as functions.
    // Starting from ed.3 all new constructors are throwing when called without |new|.
    //
    // All MozIntl APIs do not implement the legacy behavior and throw
    // when called without |new|.
    //
    // For more information see https://github.com/tc39/ecma402/pull/84 .
    Services.intl.DateTimeFormat();
  }, /class constructors must be invoked with |new|/);
}
