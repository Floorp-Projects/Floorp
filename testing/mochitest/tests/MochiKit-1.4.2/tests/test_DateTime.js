if (typeof(dojo) != 'undefined') { dojo.require('MochiKit.DateTime'); }
if (typeof(JSAN) != 'undefined') { JSAN.use('MochiKit.DateTime'); }
if (typeof(tests) == 'undefined') { tests = {}; }

tests.test_DateTime = function (t) {
    var testDate = isoDate('2005-2-3');
    t.is(testDate.getFullYear(), 2005, "isoDate year ok");
    t.is(testDate.getDate(), 3, "isoDate day ok");
    t.is(testDate.getMonth(), 1, "isoDate month ok");
    t.ok(objEqual(testDate, new Date("February 3, 2005")), "matches string date");
    t.is(toISODate(testDate), '2005-02-03', 'toISODate ok');

    var testDate = isoDate('2005-06-08');
    t.is(testDate.getFullYear(), 2005, "isoDate year ok");
    t.is(testDate.getDate(), 8, "isoDate day ok");
    t.is(testDate.getMonth(), 5, "isoDate month ok");
    t.ok(objEqual(testDate, new Date("June 8, 2005")), "matches string date");
    t.is(toISODate(testDate), '2005-06-08', 'toISODate ok');

	var testDate = isoDate('0500-12-12');
	t.is(testDate.getFullYear(), 500, 'isoDate year ok for year < 1000');
	t.is(testDate.getDate(), 12, 'isoDate day ok for year < 1000');
	t.is(testDate.getMonth(), 11, 'isoDate month ok for year < 1000');
	t.ok(objEqual(testDate, new Date("December 12, 0500")), "matches string date for year < 1000");
	t.is(toISODate(testDate), '0500-12-12', 'toISODate ok for year < 1000');

    t.is(compare(new Date("February 3, 2005"), new Date(2005, 1, 3)), 0, "dates compare eq");
    t.is(compare(new Date("February 3, 2005"), new Date(2005, 2, 3)), -1, "dates compare lt");
    t.is(compare(new Date("February 3, 2005"), new Date(2005, 0, 3)), 1, "dates compare gt");

    var testDate = isoDate('2005-2-3');
    t.is(compare(americanDate('2/3/2005'), testDate), 0, "americanDate eq");
    t.is(compare('2/3/2005', toAmericanDate(testDate)), 0, "toAmericanDate eq");

    var testTimestamp = isoTimestamp('2005-2-3 22:01:03');
    t.is(compare(testTimestamp, new Date(2005,1,3,22,1,3)), 0, "isoTimestamp eq");
    t.is(compare(testTimestamp, isoTimestamp('2005-2-3T22:01:03')), 0, "isoTimestamp (real ISO) eq");
    t.is(compare(toISOTimestamp(testTimestamp), '2005-02-03 22:01:03'), 0, "toISOTimestamp eq");
    testTimestamp = isoTimestamp('2005-2-3T22:01:03Z');
    t.is(toISOTimestamp(testTimestamp, true), '2005-02-03T22:01:03Z', "toISOTimestamp (real ISO) eq");

    var localTZ = Math.round((new Date(2005,1,3,22,1,3)).getTimezoneOffset()/60)
    var direction = (localTZ < 0) ? "+" : "-";
    localTZ = Math.abs(localTZ);
    localTZ = direction + ((localTZ < 10) ? "0" : "") + localTZ;
    testTimestamp = isoTimestamp("2005-2-3T22:01:03" + localTZ);
    var testDateTimestamp = new Date(2005,1,3,22,1,3);
    t.is(compare(testTimestamp, testDateTimestamp), 0, "equal with local tz");
    testTimestamp = isoTimestamp("2005-2-3T17:01:03-05");
    var testDateTimestamp = new Date(Date.UTC(2005,1,3,22,1,3));
    t.is(compare(testTimestamp, testDateTimestamp), 0, "equal with specific tz");
};
