/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  const mozIntl = Components.classes["@mozilla.org/mozintl;1"]
                            .getService(Components.interfaces.mozIMozIntl);

  test_methods_presence(mozIntl);
  test_methods_calling(mozIntl);

  ok(true);
}

function test_methods_presence(mozIntl) {
  equal(mozIntl.getCalendarInfo instanceof Function, true);
  equal(mozIntl.getDisplayNames instanceof Function, true);
  equal(mozIntl.getLocaleInfo instanceof Function, true);
  equal(mozIntl.createPluralRules instanceof Function, true);
  equal(mozIntl.createDateTimeFormat instanceof Function, true);
}

function test_methods_calling(mozIntl) {
  mozIntl.getCalendarInfo("pl");
  mozIntl.getDisplayNames("ar");
  mozIntl.getLocaleInfo("de");
  mozIntl.createPluralRules("fr");
  mozIntl.createDateTimeFormat("fr");
  ok(true);
}
