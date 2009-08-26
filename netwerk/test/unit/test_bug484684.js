function getURLContent(aURL) {
  var ios = Components.classes["@mozilla.org/network/io-service;1"].
            getService(Components.interfaces.nsIIOService);

  var uri = ios.newURI(aURL, null, null);
  var chan = ios.newChannelFromURI(uri);
  var inp = chan.open();
  var scinp = Components.classes["@mozilla.org/scriptableinputstream;1"].
              createInstance(Components.interfaces.nsIScriptableInputStream);
  scinp.init(inp);
  var result = "";
  var avail = scinp.available();
  while (avail) {
    result += scinp.read(avail);
    avail = scinp.available();
  }
  return result;
}

function storeCache(aURL, aContent) {
  const nsICache = Components.interfaces.nsICache;

  var cache = Components.classes["@mozilla.org/network/cache-service;1"].
              getService(Components.interfaces.nsICacheService);

  var session = cache.createSession("FTP", nsICache.STORE_ANYWHERE, nsICache.STREAM_BASED);

  var cacheEntry = session.openCacheEntry(aURL, nsICache.ACCESS_READ_WRITE, false);

  cacheEntry.setMetaDataElement("servertype", "0");
  var oStream = cacheEntry.openOutputStream(0);

  var written = oStream.write(aContent, aContent.length);
  if (written != aContent.length) {
    do_throw("oStream.write has not written all data!\n" +
             "  Expected: " + written  + "\n" +
             "  Actual: " + aContent.length + "\n");
  }
  oStream.close();
  cacheEntry.close();
}

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

function run_test() {
  for (var i = 0; i < tests.length; i++) {
    storeCache(URL, tests[i][0]);
    var parsedData = getURLContent(URL);
    do_check_eq(parsedData, tests[i][1]);
  }
}
