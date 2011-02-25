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

function run_test() {
  for (var i = 0; i < tests.length; i++) {
    storeCache(URL, tests[i][0]);
    var parsedData = getURLContent(URL);
    do_check_eq(parsedData, tests[i][1]);
  }
}
