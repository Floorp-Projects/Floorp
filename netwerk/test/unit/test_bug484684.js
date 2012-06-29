const URL = "ftp://localhost/bug464884/";

const tests = [
  // standard ls unix format
  ["-rw-rw-r--    1 500      500             0 Jan 01  2000 file1\r\n" +
   "-rw-rw-r--    1 500      500             0 Jan 01  2000  file2\r\n",

   "300: " + URL + "\n" +
   "200: filename content-length last-modified file-type\n" +
   "201: \"file1\" 0 Sun%2C%2001%20Jan%202000%2000%3A00%3A00 FILE \n" +
   "201: \"%20file2\" 0 Sun%2C%2001%20Jan%202000%2000%3A00%3A00 FILE \n"],
  // old Hellsoft unix format
  ["-[RWCEMFA] supervisor         214059       Jan 01  2000    file1\r\n" +
   "-[RWCEMFA] supervisor         214059       Jan 01  2000     file2\r\n",

   "300: " + URL + "\n" +
   "200: filename content-length last-modified file-type\n" +
   "201: \"file1\" 214059 Sun%2C%2001%20Jan%202000%2000%3A00%3A00 FILE \n" +
   "201: \"file2\" 214059 Sun%2C%2001%20Jan%202000%2000%3A00%3A00 FILE \n"],
  // new Hellsoft unix format
  ["- [RWCEAFMS] jrd                    192 Jan 01  2000 file1\r\n"+
   "- [RWCEAFMS] jrd                    192 Jan 01  2000  file2\r\n",

   "300: " + URL + "\n" +
   "200: filename content-length last-modified file-type\n" +
   "201: \"file1\" 192 Sun%2C%2001%20Jan%202000%2000%3A00%3A00 FILE \n" +
   "201: \"%20file2\" 192 Sun%2C%2001%20Jan%202000%2000%3A00%3A00 FILE \n"],
  // DOS format with correct offsets
  ["01-01-00  01:00AM       <DIR>          dir1\r\n" +
   "01-01-00  01:00AM       <JUNCTION>     junction1 -> foo1\r\n" +
   "01-01-00  01:00AM                95077 file1\r\n" +
   "01-01-00  01:00AM       <DIR>           dir2\r\n" +
   "01-01-00  01:00AM       <JUNCTION>      junction2 ->  foo2\r\n" +
   "01-01-00  01:00AM                95077  file2\r\n",

   "300: " + URL + "\n" +
   "200: filename content-length last-modified file-type\n" +
   "201: \"dir1\" 0 Sun%2C%2001%20Jan%202000%2001%3A00%3A00 DIRECTORY \n" +
   "201: \"junction1\"  Sun%2C%2001%20Jan%202000%2001%3A00%3A00 SYMBOLIC-LINK \n" +
   "201: \"file1\" 95077 Sun%2C%2001%20Jan%202000%2001%3A00%3A00 FILE \n" +
   "201: \"%20dir2\" 0 Sun%2C%2001%20Jan%202000%2001%3A00%3A00 DIRECTORY \n" +
   "201: \"%20junction2\"  Sun%2C%2001%20Jan%202000%2001%3A00%3A00 SYMBOLIC-LINK \n" +
   "201: \"%20file2\" 95077 Sun%2C%2001%20Jan%202000%2001%3A00%3A00 FILE \n"],
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
   "201: \"dir1\" 0 Sun%2C%2001%20Jan%202000%2001%3A00%3A00 DIRECTORY \n" +
   "201: \"dir2\" 0 Sun%2C%2001%20Jan%202000%2001%3A00%3A00 DIRECTORY \n" +
   "201: \"dir3\" 0 Sun%2C%2001%20Jan%202000%2001%3A00%3A00 DIRECTORY \n" +
   "201: \"junction1\"  Sun%2C%2001%20Jan%202000%2001%3A00%3A00 SYMBOLIC-LINK \n" +
   "201: \"junction2\"  Sun%2C%2001%20Jan%202000%2001%3A00%3A00 SYMBOLIC-LINK \n" +
   "201: \"junction3\"  Sun%2C%2001%20Jan%202000%2001%3A00%3A00 SYMBOLIC-LINK \n" +
   "201: \"file1\" 95077 Sun%2C%2001%20Jan%202000%2001%3A00%3A00 FILE \n" +
   "201: \"file2\" 95077 Sun%2C%2001%20Jan%202000%2001%3A00%3A00 FILE \n"]
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
