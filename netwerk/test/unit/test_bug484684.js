const URL = "ftp://localhost/bug464884/";
Cu.import("resource://gre/modules/NetUtil.jsm");

const tests = [
  // standard ls unix format
  ["-rw-rw-r--    1 500      500             0 Jan 01  2000 file1\r\n" +
   "-rw-rw-r--    1 500      500             0 Jan 01  2000  file2\r\n",

   "300: " + URL + "\n" +
   "200: filename content-length last-modified file-type\n" +
   "201: \"file1\" 0 Sat%2C%2001%20Jan%202000%2000%3A00%3A00 FILE \n" +
   "201: \"%20file2\" 0 Sat%2C%2001%20Jan%202000%2000%3A00%3A00 FILE \n"],
  // old Hellsoft unix format
  ["-[RWCEMFA] supervisor         214059       Jan 01  2000    file1\r\n" +
   "-[RWCEMFA] supervisor         214059       Jan 01  2000     file2\r\n",

   "300: " + URL + "\n" +
   "200: filename content-length last-modified file-type\n" +
   "201: \"file1\" 214059 Sat%2C%2001%20Jan%202000%2000%3A00%3A00 FILE \n" +
   "201: \"file2\" 214059 Sat%2C%2001%20Jan%202000%2000%3A00%3A00 FILE \n"],
  // new Hellsoft unix format
  ["- [RWCEAFMS] jrd                    192 Jan 01  2000 file1\r\n"+
   "- [RWCEAFMS] jrd                    192 Jan 01  2000  file2\r\n",

   "300: " + URL + "\n" +
   "200: filename content-length last-modified file-type\n" +
   "201: \"file1\" 192 Sat%2C%2001%20Jan%202000%2000%3A00%3A00 FILE \n" +
   "201: \"%20file2\" 192 Sat%2C%2001%20Jan%202000%2000%3A00%3A00 FILE \n"],
  // DOS format with correct offsets
  ["01-01-00  01:00AM       <DIR>          dir1\r\n" +
   "01-01-00  01:00AM       <JUNCTION>     junction1 -> foo1\r\n" +
   "01-01-00  01:00AM                95077 file1\r\n" +
   "01-01-00  01:00AM       <DIR>           dir2\r\n" +
   "01-01-00  01:00AM       <JUNCTION>      junction2 ->  foo2\r\n" +
   "01-01-00  01:00AM                95077  file2\r\n",

   "300: " + URL + "\n" +
   "200: filename content-length last-modified file-type\n" +
   "201: \"dir1\" 0 Sat%2C%2001%20Jan%202000%2001%3A00%3A00 DIRECTORY \n" +
   "201: \"junction1\"  Sat%2C%2001%20Jan%202000%2001%3A00%3A00 SYMBOLIC-LINK \n" +
   "201: \"file1\" 95077 Sat%2C%2001%20Jan%202000%2001%3A00%3A00 FILE \n" +
   "201: \"%20dir2\" 0 Sat%2C%2001%20Jan%202000%2001%3A00%3A00 DIRECTORY \n" +
   "201: \"%20junction2\"  Sat%2C%2001%20Jan%202000%2001%3A00%3A00 SYMBOLIC-LINK \n" +
   "201: \"%20file2\" 95077 Sat%2C%2001%20Jan%202000%2001%3A00%3A00 FILE \n"],
  // DOS format with wrong offsets
  ["01-01-00  01:00AM       <DIR>       dir1\r\n" +
   "01-01-00  01:00AM     <DIR>             dir2\r\n" +
   "01-01-00  01:00AM   <DIR>                  dir3\r\n" +
   "01-01-00  01:00AM       <JUNCTION>  junction1 -> foo1\r\n" +
   "01-01-00  01:00AM     <JUNCTION>        junction2 ->  foo2\r\n" +
   "01-01-00  01:00AM   <JUNCTION>             junction3 ->  foo3\r\n" +
   "01-01-00  01:00AM               95077  file1\r\n" +
   "01-01-00  01:00AM        95077 file2\r\n",

   "300: " + URL + "\n" +
   "200: filename content-length last-modified file-type\n" +
   "201: \"dir1\" 0 Sat%2C%2001%20Jan%202000%2001%3A00%3A00 DIRECTORY \n" +
   "201: \"dir2\" 0 Sat%2C%2001%20Jan%202000%2001%3A00%3A00 DIRECTORY \n" +
   "201: \"dir3\" 0 Sat%2C%2001%20Jan%202000%2001%3A00%3A00 DIRECTORY \n" +
   "201: \"junction1\"  Sat%2C%2001%20Jan%202000%2001%3A00%3A00 SYMBOLIC-LINK \n" +
   "201: \"junction2\"  Sat%2C%2001%20Jan%202000%2001%3A00%3A00 SYMBOLIC-LINK \n" +
   "201: \"junction3\"  Sat%2C%2001%20Jan%202000%2001%3A00%3A00 SYMBOLIC-LINK \n" +
   "201: \"file1\" 95077 Sat%2C%2001%20Jan%202000%2001%3A00%3A00 FILE \n" +
   "201: \"file2\" 95077 Sat%2C%2001%20Jan%202000%2001%3A00%3A00 FILE \n"]
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
