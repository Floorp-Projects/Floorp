/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let Cu = Components.utils;
Cu.import("resource://gre/modules/DownloadUtils.jsm");

const gDecimalSymbol = Number(5.4).toLocaleString().match(/\D/);
function _(str) {
  return str.replace(/\./g, gDecimalSymbol);
}

function testConvertByteUnits(aBytes, aValue, aUnit)
{
  let [value, unit] = DownloadUtils.convertByteUnits(aBytes);
  do_check_eq(value, aValue);
  do_check_eq(unit, aUnit);
}

function testTransferTotal(aCurrBytes, aMaxBytes, aTransfer)
{
  let transfer = DownloadUtils.getTransferTotal(aCurrBytes, aMaxBytes);
  do_check_eq(transfer, aTransfer);
}

// Get the em-dash character because typing it directly here doesn't work :(
let gDash = DownloadUtils.getDownloadStatus(0)[0].match(/remaining (.) 0 bytes/)[1];

let gVals = [0, 100, 2345, 55555, 982341, 23194134, 1482, 58, 9921949201, 13498132, Infinity];

function testStatus(aFunc, aCurr, aMore, aRate, aTest)
{
  dump("Status Test: " + [aCurr, aMore, aRate, aTest] + "\n");
  let curr = gVals[aCurr];
  let max = curr + gVals[aMore];
  let speed = gVals[aRate];

  let [status, last] = aFunc(curr, max, speed);

  if (0) {
    dump("testStatus(" + aCurr + ", " + aMore + ", " + aRate + ", [\"" +
      status.replace(gDash, "--") + "\", " + last.toFixed(3) + "]);\n");
  }

  // Make sure the status text matches
  do_check_eq(status, _(aTest[0].replace(/--/, gDash)));

  // Make sure the lastSeconds matches
  if (last == Infinity)
    do_check_eq(last, aTest[1]);
  else
    do_check_true(Math.abs(last - aTest[1]) < .1);
}

function testURI(aURI, aDisp, aHost)
{
  dump("URI Test: " + [aURI, aDisp, aHost] + "\n");

  let [disp, host] = DownloadUtils.getURIHost(aURI);

  // Make sure we have the right display host and full host
  do_check_eq(disp, aDisp);
  do_check_eq(host, aHost);
}


function testGetReadableDates(aDate, aCompactValue)
{
  const now = new Date(2000, 11, 31, 11, 59, 59);

  let [dateCompact] = DownloadUtils.getReadableDates(aDate, now);
  do_check_eq(dateCompact, aCompactValue);
}

function testAllGetReadableDates()
{
  // This test cannot depend on the current date and time, or the date format.
  // It depends on being run with the English localization, however.
  const today_11_30     = new Date(2000, 11, 31, 11, 30, 15);
  const today_12_30     = new Date(2000, 11, 31, 12, 30, 15);
  const yesterday_11_30 = new Date(2000, 11, 30, 11, 30, 15);
  const yesterday_12_30 = new Date(2000, 11, 30, 12, 30, 15);
  const twodaysago      = new Date(2000, 11, 29, 11, 30, 15);
  const sixdaysago      = new Date(2000, 11, 25, 11, 30, 15);
  const sevendaysago    = new Date(2000, 11, 24, 11, 30, 15);

  let dts = Components.classes["@mozilla.org/intl/scriptabledateformat;1"].
            getService(Components.interfaces.nsIScriptableDateFormat);

  testGetReadableDates(today_11_30, dts.FormatTime("", dts.timeFormatNoSeconds,
                                                   11, 30, 0));
  testGetReadableDates(today_12_30, dts.FormatTime("", dts.timeFormatNoSeconds,
                                                   12, 30, 0));
  testGetReadableDates(yesterday_11_30, "Yesterday");
  testGetReadableDates(yesterday_12_30, "Yesterday");
  testGetReadableDates(twodaysago, twodaysago.toLocaleFormat("%A"));
  testGetReadableDates(sixdaysago, sixdaysago.toLocaleFormat("%A"));
  testGetReadableDates(sevendaysago, sevendaysago.toLocaleFormat("%B") + " " +
                                     sevendaysago.toLocaleFormat("%d"));

  let [, dateTimeFull] = DownloadUtils.getReadableDates(today_11_30);
  do_check_eq(dateTimeFull, dts.FormatDateTime("", dts.dateFormatLong,
                                                   dts.timeFormatNoSeconds,
                                                   2000, 12, 31, 11, 30, 0));
}

function run_test()
{
  testConvertByteUnits(-1, "-1", "bytes");
  testConvertByteUnits(1, _("1"), "bytes");
  testConvertByteUnits(42, _("42"), "bytes");
  testConvertByteUnits(123, _("123"), "bytes");
  testConvertByteUnits(1024, _("1.0"), "KB");
  testConvertByteUnits(8888, _("8.7"), "KB");
  testConvertByteUnits(59283, _("57.9"), "KB");
  testConvertByteUnits(640000, _("625"), "KB");
  testConvertByteUnits(1048576, _("1.0"), "MB");
  testConvertByteUnits(307232768, _("293"), "MB");
  testConvertByteUnits(1073741824, _("1.0"), "GB");

  testTransferTotal(1, 1, _("1 of 1 bytes"));
  testTransferTotal(234, 4924, _("234 bytes of 4.8 KB"));
  testTransferTotal(94923, 233923, _("92.7 of 228 KB"));
  testTransferTotal(4924, 94923, _("4.8 of 92.7 KB"));
  testTransferTotal(2342, 294960345, _("2.3 KB of 281 MB"));
  testTransferTotal(234, undefined, _("234 bytes"));
  testTransferTotal(4889023, undefined, _("4.7 MB"));

  if (0) {
    // Help find some interesting test cases
    let r = function() Math.floor(Math.random() * 10);
    for (let i = 0; i < 100; i++) {
      testStatus(r(), r(), r());
    }
  }

  // First, test with rates, via getDownloadStatus...
  let statusFunc = DownloadUtils.getDownloadStatus.bind(DownloadUtils);

  testStatus(statusFunc, 2, 1, 7, ["A few seconds remaining -- 2.3 of 2.4 KB (58 bytes/sec)", 1.724]);
  testStatus(statusFunc, 1, 2, 6, ["A few seconds remaining -- 100 bytes of 2.4 KB (1.4 KB/sec)", 1.582]);
  testStatus(statusFunc, 4, 3, 9, ["A few seconds remaining -- 959 KB of 1.0 MB (12.9 MB/sec)", 0.004]);
  testStatus(statusFunc, 2, 3, 8, ["A few seconds remaining -- 2.3 of 56.5 KB (9.2 GB/sec)", 0.000]);

  testStatus(statusFunc, 8, 4, 3, ["17 seconds remaining -- 9.2 of 9.2 GB (54.3 KB/sec)", 17.682]);
  testStatus(statusFunc, 1, 3, 2, ["23 seconds remaining -- 100 bytes of 54.4 KB (2.3 KB/sec)", 23.691]);
  testStatus(statusFunc, 9, 3, 2, ["23 seconds remaining -- 12.9 of 12.9 MB (2.3 KB/sec)", 23.691]);
  testStatus(statusFunc, 5, 6, 7, ["25 seconds remaining -- 22.1 of 22.1 MB (58 bytes/sec)", 25.552]);

  testStatus(statusFunc, 3, 9, 3, ["4 minutes remaining -- 54.3 KB of 12.9 MB (54.3 KB/sec)", 242.969]);
  testStatus(statusFunc, 2, 3, 1, ["9 minutes remaining -- 2.3 of 56.5 KB (100 bytes/sec)", 555.550]);
  testStatus(statusFunc, 4, 3, 7, ["15 minutes remaining -- 959 KB of 1.0 MB (58 bytes/sec)", 957.845]);
  testStatus(statusFunc, 5, 3, 7, ["15 minutes remaining -- 22.1 of 22.2 MB (58 bytes/sec)", 957.845]);

  testStatus(statusFunc, 1, 9, 2, ["1 hour, 35 minutes remaining -- 100 bytes of 12.9 MB (2.3 KB/sec)", 5756.133]);
  testStatus(statusFunc, 2, 9, 6, ["2 hours, 31 minutes remaining -- 2.3 KB of 12.9 MB (1.4 KB/sec)", 9108.051]);
  testStatus(statusFunc, 2, 4, 1, ["2 hours, 43 minutes remaining -- 2.3 of 962 KB (100 bytes/sec)", 9823.410]);
  testStatus(statusFunc, 6, 4, 7, ["4 hours, 42 minutes remaining -- 1.4 of 961 KB (58 bytes/sec)", 16936.914]);

  testStatus(statusFunc, 6, 9, 1, ["1 day, 13 hours remaining -- 1.4 KB of 12.9 MB (100 bytes/sec)", 134981.320]);
  testStatus(statusFunc, 3, 8, 3, ["2 days, 1 hour remaining -- 54.3 KB of 9.2 GB (54.3 KB/sec)", 178596.872]);
  testStatus(statusFunc, 1, 8, 6, ["77 days, 11 hours remaining -- 100 bytes of 9.2 GB (1.4 KB/sec)", 6694972.470]);
  testStatus(statusFunc, 6, 8, 7, ["1979 days, 22 hours remaining -- 1.4 KB of 9.2 GB (58 bytes/sec)", 171068089.672]);

  testStatus(statusFunc, 0, 0, 5, ["Unknown time remaining -- 0 of 0 bytes (22.1 MB/sec)", Infinity]);
  testStatus(statusFunc, 0, 6, 0, ["Unknown time remaining -- 0 bytes of 1.4 KB (0 bytes/sec)", Infinity]);
  testStatus(statusFunc, 6, 6, 0, ["Unknown time remaining -- 1.4 of 2.9 KB (0 bytes/sec)", Infinity]);
  testStatus(statusFunc, 8, 5, 0, ["Unknown time remaining -- 9.2 of 9.3 GB (0 bytes/sec)", Infinity]);

  // With rate equal to Infinity
  testStatus(statusFunc, 0, 0, 10, ["Unknown time remaining -- 0 of 0 bytes (Really fast)", Infinity]);
  testStatus(statusFunc, 1, 2, 10, ["A few seconds remaining -- 100 bytes of 2.4 KB (Really fast)", 0]);

  // Now test without rates, via getDownloadStatusNoRate.
  statusFunc = DownloadUtils.getDownloadStatusNoRate.bind(DownloadUtils);

  testStatus(statusFunc, 2, 1, 7, ["A few seconds remaining -- 2.3 of 2.4 KB", 1.724]);
  testStatus(statusFunc, 1, 2, 6, ["A few seconds remaining -- 100 bytes of 2.4 KB", 1.582]);
  testStatus(statusFunc, 4, 3, 9, ["A few seconds remaining -- 959 KB of 1.0 MB", 0.004]);
  testStatus(statusFunc, 2, 3, 8, ["A few seconds remaining -- 2.3 of 56.5 KB", 0.000]);

  testStatus(statusFunc, 8, 4, 3, ["17 seconds remaining -- 9.2 of 9.2 GB", 17.682]);
  testStatus(statusFunc, 1, 3, 2, ["23 seconds remaining -- 100 bytes of 54.4 KB", 23.691]);
  testStatus(statusFunc, 9, 3, 2, ["23 seconds remaining -- 12.9 of 12.9 MB", 23.691]);
  testStatus(statusFunc, 5, 6, 7, ["25 seconds remaining -- 22.1 of 22.1 MB", 25.552]);

  testStatus(statusFunc, 3, 9, 3, ["4 minutes remaining -- 54.3 KB of 12.9 MB", 242.969]);
  testStatus(statusFunc, 2, 3, 1, ["9 minutes remaining -- 2.3 of 56.5 KB", 555.550]);
  testStatus(statusFunc, 4, 3, 7, ["15 minutes remaining -- 959 KB of 1.0 MB", 957.845]);
  testStatus(statusFunc, 5, 3, 7, ["15 minutes remaining -- 22.1 of 22.2 MB", 957.845]);

  testStatus(statusFunc, 1, 9, 2, ["1 hour, 35 minutes remaining -- 100 bytes of 12.9 MB", 5756.133]);
  testStatus(statusFunc, 2, 9, 6, ["2 hours, 31 minutes remaining -- 2.3 KB of 12.9 MB", 9108.051]);
  testStatus(statusFunc, 2, 4, 1, ["2 hours, 43 minutes remaining -- 2.3 of 962 KB", 9823.410]);
  testStatus(statusFunc, 6, 4, 7, ["4 hours, 42 minutes remaining -- 1.4 of 961 KB", 16936.914]);

  testStatus(statusFunc, 6, 9, 1, ["1 day, 13 hours remaining -- 1.4 KB of 12.9 MB", 134981.320]);
  testStatus(statusFunc, 3, 8, 3, ["2 days, 1 hour remaining -- 54.3 KB of 9.2 GB", 178596.872]);
  testStatus(statusFunc, 1, 8, 6, ["77 days, 11 hours remaining -- 100 bytes of 9.2 GB", 6694972.470]);
  testStatus(statusFunc, 6, 8, 7, ["1979 days, 22 hours remaining -- 1.4 KB of 9.2 GB", 171068089.672]);

  testStatus(statusFunc, 0, 0, 5, ["Unknown time remaining -- 0 of 0 bytes", Infinity]);
  testStatus(statusFunc, 0, 6, 0, ["Unknown time remaining -- 0 bytes of 1.4 KB", Infinity]);
  testStatus(statusFunc, 6, 6, 0, ["Unknown time remaining -- 1.4 of 2.9 KB", Infinity]);
  testStatus(statusFunc, 8, 5, 0, ["Unknown time remaining -- 9.2 of 9.3 GB", Infinity]);

  testURI("http://www.mozilla.org/", "mozilla.org", "www.mozilla.org");
  testURI("http://www.city.mikasa.hokkaido.jp/", "city.mikasa.hokkaido.jp", "www.city.mikasa.hokkaido.jp");
  testURI("data:text/html,Hello World", "data resource", "data resource");
  testURI("jar:http://www.mozilla.com/file!/magic", "mozilla.com", "www.mozilla.com");
  testURI("file:///C:/Cool/Stuff/", "local file", "local file");
  // Don't test for moz-icon if we don't have a protocol handler for it (e.g. b2g):
  if ("@mozilla.org/network/protocol;1?name=moz-icon" in Components.classes) {
    testURI("moz-icon:file:///test.extension", "local file", "local file");
    testURI("moz-icon://.extension", "moz-icon resource", "moz-icon resource");
  }
  testURI("about:config", "about resource", "about resource");

  testAllGetReadableDates();
}
