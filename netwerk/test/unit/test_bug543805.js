const URL = "ftp://localhost/bug543805/";

var year = new Date().getFullYear().toString();

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
   "201: \"%20nodup.file\" 11 Sun%2C%2001%20Jan%20" + year + "%2020%3A19%3A00 FILE \n" +
   "201: \"%20test.blankfile\" 22 Sun%2C%2001%20Jan%20" + year + "%2020%3A19%3A00 FILE \n" +
   "201: \"%20test2.blankfile\" 33 Sun%2C%2001%20Apr%202008%2000%3A00%3A00 FILE \n" +
   "201: \"nodup.file\" 44 Sun%2C%2001%20Jan%20" + year + "%2020%3A19%3A00 FILE \n" +
   "201: \"test.file\" 55 Sun%2C%2001%20Jan%20" + year + "%2020%3A19%3A00 FILE \n" +
   "201: \"test2.file\" 66 Sun%2C%2001%20Apr%202008%2000%3A00%3A00 FILE \n"],

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
   "201: \"%20nodup.file\" 11 Sun%2C%2001%20Jan%20" + year + "%2020%3A19%3A00 FILE \n" +
   "201: \"%20test.blankfile\" 22 Sun%2C%2001%20Jan%20" + year + "%2020%3A19%3A00 FILE \n" +
   "201: \"%20test2.blankfile\" 33 Sun%2C%2001%20Apr%202008%2000%3A00%3A00 FILE \n" +
   "201: \"nodup.file\" 44 Sun%2C%2001%20Jan%20" + year + "%2020%3A19%3A00 FILE \n" +
   "201: \"test.file\" 55 Sun%2C%2001%20Jan%20" + year + "%2020%3A19%3A00 FILE \n" +
   "201: \"test2.file\" 66 Sun%2C%2001%20Apr%202008%2000%3A00%3A00 FILE \n"]
]

function checkData(request, data, ctx) {
  do_check_eq(tests[0][1], data);
  tests.shift();
  next_test();
}

function storeData(status, entry) {
  do_check_eq(status, Components.results.NS_OK);
  entry.setMetaDataElement("servertype", "0");
  var os = entry.openOutputStream(0);

  var written = os.write(tests[0][0], tests[0][0].length);
  if (written != tests[0][0].length) {
    do_throw("os.write has not written all data!\n" +
             "  Expected: " + written  + "\n" +
             "  Actual: " + tests[0][0].length + "\n");
  }
  os.close();
  entry.close();

  var ios = Components.classes["@mozilla.org/network/io-service;1"].
            getService(Components.interfaces.nsIIOService);
  var channel = ios.newChannel(URL, "", null);
  channel.asyncOpen(new ChannelListener(checkData, null, CL_ALLOW_UNKNOWN_CL), null);
}

function next_test() {
  if (tests.length == 0)
    do_test_finished();
  else {
    asyncOpenCacheEntry(URL,
                        "FTP",
                        Components.interfaces.nsICache.STORE_ANYWHERE,
                        Components.interfaces.nsICache.ACCESS_READ_WRITE,
                        storeData);
  }
}

function run_test() {
  do_execute_soon(next_test);
  do_test_pending();
}
