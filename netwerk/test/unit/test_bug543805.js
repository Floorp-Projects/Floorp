const URL = "ftp://localhost/bug543805/";
Cu.import("resource://gre/modules/NetUtil.jsm");

var dayNames = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
var year = new Date().getFullYear().toString();
var day = dayNames[new Date(year, 0, 1).getDay()];

const tests = [
  // AIX ls format
  ["-rw-r--r--   1 0                11 Jan  1 20:19  nodup.file\r\n" +
   "-rw-r--r--   1 0                22 Jan  1 20:19  test.blankfile\r\n" +
   "-rw-r--r--   1 0                33 Apr  1 2008   test2.blankfile\r\n" +
   "-rw-r--r--   1 0                44 Jan  1 20:19 nodup.file\r\n" +
   "-rw-r--r--   1 0                55 Jan  1 20:19 test.file\r\n" +
   "-rw-r--r--   1 0                66 Apr  1 2008  test2.file\r\n",

   "300: " + URL + "\n" +
   "200: filename content-length last-modified file-type\n" +
   "201: \"%20nodup.file\" 11 " + day + "%2C%2001%20Jan%20" + year + "%2020%3A19%3A00 FILE \n" +
   "201: \"%20test.blankfile\" 22 " + day + "%2C%2001%20Jan%20" + year + "%2020%3A19%3A00 FILE \n" +
   "201: \"%20test2.blankfile\" 33 Tue%2C%2001%20Apr%202008%2000%3A00%3A00 FILE \n" +
   "201: \"nodup.file\" 44 " + day + "%2C%2001%20Jan%20" + year + "%2020%3A19%3A00 FILE \n" +
   "201: \"test.file\" 55 " + day + "%2C%2001%20Jan%20" + year + "%2020%3A19%3A00 FILE \n" +
   "201: \"test2.file\" 66 Tue%2C%2001%20Apr%202008%2000%3A00%3A00 FILE \n"],

  // standard ls format
  [
   "-rw-r--r--    1 500      500            11 Jan  1 20:19  nodup.file\r\n" +
   "-rw-r--r--    1 500      500            22 Jan  1 20:19  test.blankfile\r\n" +
   "-rw-r--r--    1 500      500            33 Apr  1  2008  test2.blankfile\r\n" +
   "-rw-r--r--    1 500      500            44 Jan  1 20:19 nodup.file\r\n" +
   "-rw-r--r--    1 500      500            55 Jan  1 20:19 test.file\r\n" +
   "-rw-r--r--    1 500      500            66 Apr  1  2008 test2.file\r\n",

   "300: " + URL + "\n" +
   "200: filename content-length last-modified file-type\n" +
   "201: \"%20nodup.file\" 11 " + day + "%2C%2001%20Jan%20" + year + "%2020%3A19%3A00 FILE \n" +
   "201: \"%20test.blankfile\" 22 " + day + "%2C%2001%20Jan%20" + year + "%2020%3A19%3A00 FILE \n" +
   "201: \"%20test2.blankfile\" 33 Tue%2C%2001%20Apr%202008%2000%3A00%3A00 FILE \n" +
   "201: \"nodup.file\" 44 " + day + "%2C%2001%20Jan%20" + year + "%2020%3A19%3A00 FILE \n" +
   "201: \"test.file\" 55 " + day + "%2C%2001%20Jan%20" + year + "%2020%3A19%3A00 FILE \n" +
   "201: \"test2.file\" 66 Tue%2C%2001%20Apr%202008%2000%3A00%3A00 FILE \n"]
]

function checkData(request, data, ctx) {
  Assert.equal(tests[0][1], data);
  tests.shift();
  executeSoon(next_test);
}

function storeData(status, entry) {
  var scs = Cc["@mozilla.org/streamConverters;1"].
            getService(Ci.nsIStreamConverterService);
  var converter = scs.asyncConvertData("text/ftp-dir", "application/http-index-format",
                                       new ChannelListener(checkData, null, CL_ALLOW_UNKNOWN_CL), null);

  var stream = Cc["@mozilla.org/io/string-input-stream;1"].
               createInstance(Ci.nsIStringInputStream);
  stream.data = tests[0][0];

  var url = NetUtil.newURI(URL);

  var channel = {
    URI: url,
    contentLength: -1,
    pending: true,
    isPending: function() {
      return this.pending;
    },
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIChannel])
  };

  converter.onStartRequest(channel, null);
  converter.onDataAvailable(channel, null, stream, 0, 0);
  channel.pending = false;
  converter.onStopRequest(channel, null, Cr.NS_OK);
}

function next_test() {
  if (tests.length == 0)
    do_test_finished();
  else {
    storeData();
  }
}

function run_test() {
  executeSoon(next_test);
  do_test_pending();
}
