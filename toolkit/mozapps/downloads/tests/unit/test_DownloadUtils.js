/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is DownloadUtils Test Code.
 *
 * The Initial Developer of the Original Code is
 * Edward Lee <edward.lee@engineering.uiuc.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

let Cu = Components.utils;
Cu.import("resource://gre/modules/DownloadUtils.jsm");

const gDecimalSymbol = Number(5.4).toLocaleString().match(/\D/);
function _(str) {
  return str.replace(".", gDecimalSymbol);
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

let gVals = [0, 100, 2345, 55555, 982341, 23194134, 1482, 58, 9921949201, 13498132];

function testStatus(aCurr, aMore, aRate, aTest)
{
  dump("Status Test: " + [aCurr, aMore, aRate, aTest] + "\n");
  let curr = gVals[aCurr];
  let max = curr + gVals[aMore];
  let speed = gVals[aRate];

  let [status, last] =
    DownloadUtils.getDownloadStatus(curr, max, speed);

  if (0) {
    dump("testStatus(" + aCurr + ", " + aMore + ", " + aRate + ", [\"" +
      status.replace(gDash, "--") + "\", " + last.toFixed(3) + "]);\n");
  }

  // Make sure the status text matches
  do_check_eq(status, aTest[0].replace(/--/, gDash));

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

function run_test()
{
  testConvertByteUnits(-1, "-1", "bytes");
  testConvertByteUnits(1, _("1.0"), "bytes");
  testConvertByteUnits(42, _("42.0"), "bytes");
  testConvertByteUnits(123, _("123"), "bytes");
  testConvertByteUnits(1024, _("1.0"), "KB");
  testConvertByteUnits(8888, _("8.7"), "KB");
  testConvertByteUnits(59283, _("57.9"), "KB");
  testConvertByteUnits(640000, _("625"), "KB");
  testConvertByteUnits(1048576, _("1.0"), "MB");
  testConvertByteUnits(307232768, _("293"), "MB");
  testConvertByteUnits(1073741824, _("1.0"), "GB");

  testTransferTotal(1, 1, _("1.0 of 1.0 bytes"));
  testTransferTotal(234, 4924, _("234 bytes of 4.8 KB"));
  testTransferTotal(94923, 233923, _("92.7 of 228 KB"));
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

  testStatus(2, 1, 7, ["A few seconds remaining -- 2.3 of 2.4 KB (58.0 bytes/sec)", 1.724]);
  testStatus(1, 2, 6, ["A few seconds remaining -- 100 bytes of 2.4 KB (1.4 KB/sec)", 1.582]);
  testStatus(4, 3, 9, ["A few seconds remaining -- 959 KB of 1.0 MB (12.9 MB/sec)", 0.004]);
  testStatus(2, 3, 8, ["A few seconds remaining -- 2.3 of 56.5 KB (9.2 GB/sec)", 0.000]);

  testStatus(8, 4, 3, ["17 seconds remaining -- 9.2 of 9.2 GB (54.3 KB/sec)", 17.682]);
  testStatus(1, 3, 2, ["23 seconds remaining -- 100 bytes of 54.4 KB (2.3 KB/sec)", 23.691]);
  testStatus(9, 3, 2, ["23 seconds remaining -- 12.9 of 12.9 MB (2.3 KB/sec)", 23.691]);
  testStatus(5, 6, 7, ["25 seconds remaining -- 22.1 of 22.1 MB (58.0 bytes/sec)", 25.552]);

  testStatus(3, 9, 3, ["4 minutes remaining -- 54.3 KB of 12.9 MB (54.3 KB/sec)", 242.969]);
  testStatus(2, 3, 1, ["9 minutes remaining -- 2.3 of 56.5 KB (100 bytes/sec)", 555.550]);
  testStatus(4, 3, 7, ["15 minutes remaining -- 959 KB of 1.0 MB (58.0 bytes/sec)", 957.845]);
  testStatus(5, 3, 7, ["15 minutes remaining -- 22.1 of 22.2 MB (58.0 bytes/sec)", 957.845]);

  testStatus(1, 9, 2, ["1 hour, 35 minutes remaining -- 100 bytes of 12.9 MB (2.3 KB/sec)", 5756.133]);
  testStatus(2, 9, 6, ["2 hours, 31 minutes remaining -- 2.3 KB of 12.9 MB (1.4 KB/sec)", 9108.051]);
  testStatus(2, 4, 1, ["2 hours, 43 minutes remaining -- 2.3 of 962 KB (100 bytes/sec)", 9823.410]);
  testStatus(6, 4, 7, ["4 hours, 42 minutes remaining -- 1.4 of 961 KB (58.0 bytes/sec)", 16936.914]);

  testStatus(6, 9, 1, ["1 day, 13 hours remaining -- 1.4 KB of 12.9 MB (100 bytes/sec)", 134981.320]);
  testStatus(3, 8, 3, ["2 days, 1 hour remaining -- 54.3 KB of 9.2 GB (54.3 KB/sec)", 178596.872]);
  testStatus(1, 8, 6, ["77 days, 11 hours remaining -- 100 bytes of 9.2 GB (1.4 KB/sec)", 6694972.470]);
  testStatus(6, 8, 7, ["1979 days, 22 hours remaining -- 1.4 KB of 9.2 GB (58.0 bytes/sec)", 171068089.672]);

  testStatus(0, 0, 5, ["Unknown time remaining -- 0 of 0 bytes (22.1 MB/sec)", Infinity]);
  testStatus(0, 6, 0, ["Unknown time remaining -- 0 bytes of 1.4 KB (0 bytes/sec)", Infinity]);
  testStatus(6, 6, 0, ["Unknown time remaining -- 1.4 of 2.9 KB (0 bytes/sec)", Infinity]);
  testStatus(8, 5, 0, ["Unknown time remaining -- 9.2 of 9.3 GB (0 bytes/sec)", Infinity]);

  testURI("http://www.mozilla.org/", "mozilla.org", "www.mozilla.org");
  testURI("http://www.city.mikasa.hokkaido.jp/", "city.mikasa.hokkaido.jp", "www.city.mikasa.hokkaido.jp");
  testURI("data:text/html,Hello World", "data resource", "data resource");
  testURI("jar:http://www.mozilla.com/file!/magic", "mozilla.com", "www.mozilla.com");
  testURI("file:///C:/Cool/Stuff/", "local file", "local file");
  testURI("moz-icon:file:///test.extension", "moz-icon resource", "moz-icon resource");
  testURI("about:config", "about resource", "about resource");
}
