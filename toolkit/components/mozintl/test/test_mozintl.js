/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  test_methods_presence();
  test_methods_calling();
  test_constructors();
  test_rtf_formatBestUnit();
  test_datetimeformat();
  test_getLanguageDirection();
  test_stringHasRTLChars();

  ok(true);
}

function test_methods_presence() {
  equal(Services.intl.getCalendarInfo instanceof Function, true);
  equal(Services.intl.getDisplayNamesDeprecated instanceof Function, true);
  equal(Services.intl.getLocaleDisplayNames instanceof Function, true);
}

function test_methods_calling() {
  Services.intl.getCalendarInfo("pl");
  Services.intl.getDisplayNamesDeprecated("ar", { type: "language" });
  new Services.intl.DateTimeFormat("fr");
  new Services.intl.DisplayNames("fr", { type: "language" });
  new Services.intl.ListFormat("fr");
  new Services.intl.Locale("fr");
  new Services.intl.RelativeTimeFormat("fr");
  ok(true);
}

function test_constructors() {
  let constructors = [
    "Collator",
    "DateTimeFormat",
    "ListFormat",
    "NumberFormat",
    "PluralRules",
  ];

  constructors.forEach(constructor => {
    let obj = new Intl[constructor]();
    let obj2 = new Services.intl[constructor]();

    equal(typeof obj, typeof obj2);
  });
}

function testRTFBestUnit(anchor, value, expected) {
  let rtf = new Services.intl.RelativeTimeFormat("en-US");
  deepEqual(rtf.formatBestUnit(new Date(value), { now: anchor }), expected);
}

function test_rtf_formatBestUnit() {
  {
    // format seconds-distant dates
    let anchor = new Date("2016-04-10 12:00:00");
    testRTFBestUnit(anchor, "2016-04-10 11:59:01", "59 seconds ago");
    testRTFBestUnit(anchor, "2016-04-10 12:00:00", "now");
    testRTFBestUnit(anchor, "2016-04-10 12:00:59", "in 59 seconds");
  }

  {
    // format minutes-distant dates
    let anchor = new Date("2016-04-10 12:00:00");
    testRTFBestUnit(anchor, "2016-04-10 11:01:00", "59 minutes ago");
    testRTFBestUnit(anchor, "2016-04-10 11:59", "1 minute ago");
    testRTFBestUnit(anchor, "2016-04-10 12:01", "in 1 minute");
    testRTFBestUnit(anchor, "2016-04-10 12:01:59", "in 1 minute");
    testRTFBestUnit(anchor, "2016-04-10 12:59:59", "in 59 minutes");
  }

  {
    // format hours-distant dates
    let anchor = new Date("2016-04-10 12:00:00");
    testRTFBestUnit(anchor, "2016-04-10 00:00", "12 hours ago");
    testRTFBestUnit(anchor, "2016-04-10 13:00", "in 1 hour");
    testRTFBestUnit(anchor, "2016-04-10 13:59:59", "in 1 hour");
    testRTFBestUnit(anchor, "2016-04-10 23:59:59", "in 11 hours");

    anchor = new Date("2016-04-10 01:00");
    testRTFBestUnit(anchor, "2016-04-09 19:00", "6 hours ago");
    testRTFBestUnit(anchor, "2016-04-09 18:00", "yesterday");

    anchor = new Date("2016-04-10 23:00");
    testRTFBestUnit(anchor, "2016-04-11 05:00", "in 6 hours");
    testRTFBestUnit(anchor, "2016-04-11 06:00", "tomorrow");

    anchor = new Date("2016-01-31 23:00");
    testRTFBestUnit(anchor, "2016-02-01 05:00", "in 6 hours");
    testRTFBestUnit(anchor, "2016-02-01 07:00", "tomorrow");

    anchor = new Date("2016-12-31 23:00");
    testRTFBestUnit(anchor, "2017-01-01 05:00", "in 6 hours");
    testRTFBestUnit(anchor, "2017-01-01 07:00", "tomorrow");
  }

  {
    // format days-distant dates
    let anchor = new Date("2016-04-10 12:00:00");
    testRTFBestUnit(anchor, "2016-04-01 00:00", "last week");
    testRTFBestUnit(anchor, "2016-04-05 00:00", "5 days ago");
    testRTFBestUnit(anchor, "2016-04-09 18:00", "yesterday");
    testRTFBestUnit(anchor, "2016-04-11 09:00", "tomorrow");
    testRTFBestUnit(anchor, "2016-04-30 23:59", "in 2 weeks");
    testRTFBestUnit(anchor, "2016-03-31 23:59", "last week");
    testRTFBestUnit(anchor, "2016-04-18 23:59", "next week");
    testRTFBestUnit(anchor, "2016-03-03 23:59", "last month");
    testRTFBestUnit(anchor, "2016-05-12 00:00", "next month");

    anchor = new Date("2016-04-06 12:00");
    testRTFBestUnit(anchor, "2016-03-31 23:59", "6 days ago");

    anchor = new Date("2016-04-25 23:00");
    testRTFBestUnit(anchor, "2016-05-01 00:00", "in 6 days");
  }

  {
    // format months-distant dates
    let anchor = new Date("2016-04-10 12:00:00");
    testRTFBestUnit(anchor, "2016-01-01 00:00", "3 months ago");
    testRTFBestUnit(anchor, "2016-03-01 00:00", "last month");
    testRTFBestUnit(anchor, "2016-05-11 00:00", "next month");
    testRTFBestUnit(anchor, "2016-12-01 23:59", "in 8 months");

    anchor = new Date("2017-01-12 18:30");
    testRTFBestUnit(anchor, "2016-12-14 18:30", "last month");

    anchor = new Date("2016-12-14 18:30");
    testRTFBestUnit(anchor, "2017-01-12 18:30", "next month");

    anchor = new Date("2016-02-28 12:00");
    testRTFBestUnit(anchor, "2015-12-31 23:59", "2 months ago");
  }

  {
    // format year-distant dates
    let anchor = new Date("2016-04-10 12:00:00");
    testRTFBestUnit(anchor, "2014-04-01 00:00", "2 years ago");
    testRTFBestUnit(anchor, "2015-03-01 00:00", "last year");
    testRTFBestUnit(anchor, "2015-04-01 00:00", "last year");
    testRTFBestUnit(anchor, "2015-05-01 00:00", "11 months ago");
    testRTFBestUnit(anchor, "2015-12-01 00:00", "4 months ago");
    testRTFBestUnit(anchor, "2016-02-01 00:00", "2 months ago");
    testRTFBestUnit(anchor, "2017-05-01 00:00", "next year");
    testRTFBestUnit(anchor, "2024-12-01 23:59", "in 8 years");

    anchor = new Date("2017-01-12 18:30");
    testRTFBestUnit(anchor, "2016-01-01 18:30", "last year");
    testRTFBestUnit(anchor, "2015-12-29 18:30", "2 years ago");

    anchor = new Date("2016-12-29 18:30");
    testRTFBestUnit(anchor, "2017-02-12 18:30", "in 2 months");
    testRTFBestUnit(anchor, "2017-07-12 18:30", "in 7 months");
    testRTFBestUnit(anchor, "2017-11-29 18:30", "in 11 months");
    testRTFBestUnit(anchor, "2017-12-01 18:30", "next year");
    testRTFBestUnit(anchor, "2018-01-02 18:30", "in 2 years");

    testRTFBestUnit(anchor, "2098-01-02 18:30", "in 82 years");
  }
}

function test_datetimeformat() {
  Services.prefs.setStringPref(
    "intl.date_time.pattern_override.date_long",
    "yyyy年M月d日"
  );

  let formatted = new Services.intl.DateTimeFormat("ja", {
    dateStyle: "long",
  }).format(new Date("2020-12-08 21:00:05"));

  equal(formatted, "2020年12月8日");

  Services.prefs.clearUserPref("intl.date_time.pattern_override.date_long");
}

function test_getLanguageDirection() {
  equal(Services.intl.getScriptDirection("ar"), "rtl");
  equal(Services.intl.getScriptDirection("ar-EG"), "rtl");
  equal(Services.intl.getScriptDirection("ckb"), "rtl");
  equal(Services.intl.getScriptDirection("fa"), "rtl");
  equal(Services.intl.getScriptDirection("he"), "rtl");
  equal(Services.intl.getScriptDirection("ur"), "rtl");

  equal(Services.intl.getScriptDirection("en"), "ltr");
  equal(Services.intl.getScriptDirection("en-US"), "ltr");
  equal(Services.intl.getScriptDirection("fr"), "ltr");
}

function test_stringHasRTLChars() {
  equal(Services.intl.stringHasRTLChars(""), false);
  equal(Services.intl.stringHasRTLChars("a"), false);
  equal(Services.intl.stringHasRTLChars("أهلا"), true);
  equal(Services.intl.stringHasRTLChars(">\u202e<"), true);

  const invalidArgs = [undefined, null, false, 42, {}];
  for (const invalidArg of invalidArgs) {
    try {
      Services.intl.stringHasRTLChars(invalidArg);
      ok(
        false,
        `stringHasRTLChars should throw when called with ${invalidArg}`
      );
    } catch (e) {
      ok(true, `stringHasRTLChars throws when called with ${invalidArg}`);
    }
  }
}
