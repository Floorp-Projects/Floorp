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
    do_throw("oStream.write have not written all data!\n" +
             "  Expected: " + written  + "\n" +
             "  Actual: " + aContent.length + "\n");
  }
  oStream.close();
  cacheEntry.close();
}

function run_test() {
  /* Unix style listing, space at the end of filename */
  var rawData = "drwxrwxr-x    2 500      500          4096 Jan 01  2000 a \r\n";
  storeCache("ftp://localhost/bug365133/", rawData);
  var parsedData = getURLContent("ftp://localhost/bug365133/");
  do_check_eq(parsedData,
    "300: ftp://localhost/bug365133/\n" +
    "200: filename content-length last-modified file-type\n" +
    "201: \"a%20\" 0 Sun%2C%2001%20Jan%202000%2000%3A00%3A00 DIRECTORY \n");

  /* Unix style listing, space at the end of link name */
  rawData = "lrwxrwxrwx    1 500      500             2 Jan 01  2000 l  -> a \r\n";
  storeCache("ftp://localhost/bug365133/", rawData);
  parsedData = getURLContent("ftp://localhost/bug365133/");
  do_check_eq(parsedData,
    "300: ftp://localhost/bug365133/\n" +
    "200: filename content-length last-modified file-type\n" +
    "201: \"l%20\" 2 Sun%2C%2001%20Jan%202000%2000%3A00%3A00 SYMBOLIC-LINK \n");

  /* Unix style listing, regular file with " -> " in name */
  rawData = "-rw-rw-r--    1 500      500             0 Jan 01  2000 arrow -> in name \r\n";
  storeCache("ftp://localhost/bug365133/", rawData);
  parsedData = getURLContent("ftp://localhost/bug365133/");
  do_check_eq(parsedData,
    "300: ftp://localhost/bug365133/\n" +
    "200: filename content-length last-modified file-type\n" +
    "201: \"arrow%20-%3E%20in%20name%20\" 0 Sun%2C%2001%20Jan%202000%2000%3A00%3A00 FILE \n");

  /* Unix style listing, tab at the end of filename */
  rawData = "drwxrwxrwx    2 500      500          4096 Jan 01  2000 t	\r\n";
  storeCache("ftp://localhost/bug365133/", rawData);
  parsedData = getURLContent("ftp://localhost/bug365133/");
  do_check_eq(parsedData,
    "300: ftp://localhost/bug365133/\n" +
    "200: filename content-length last-modified file-type\n" +
    "201: \"t%09\" 0 Sun%2C%2001%20Jan%202000%2000%3A00%3A00 DIRECTORY \n");

  /* Unix style listing, multiple " -> " in filename */
  rawData = "lrwxrwxrwx    1 500      500            26 Jan 01  2000 symlink with arrow -> in name -> file with arrow -> in name\r\n";
  storeCache("ftp://localhost/bug365133/", rawData);
  parsedData = getURLContent("ftp://localhost/bug365133/");
  do_check_eq(parsedData,
    "300: ftp://localhost/bug365133/\n" +
    "200: filename content-length last-modified file-type\n" +
    "201: \"symlink%20with%20arrow%20-%3E%20in%20name\" 26 Sun%2C%2001%20Jan%202000%2000%3A00%3A00 SYMBOLIC-LINK \n");

  /* Unix style listing, multiple " -> " in filename, incorrect filesize */
  rawData = "lrwxrwxrwx    1 500      500             0 Jan 01  2000 symlink with arrow -> in name -> file with arrow -> in name\r\n";
  storeCache("ftp://localhost/bug365133/", rawData);
  parsedData = getURLContent("ftp://localhost/bug365133/");
  do_check_eq(parsedData,
    "300: ftp://localhost/bug365133/\n" +
    "200: filename content-length last-modified file-type\n" +
    "201: \"symlink%20with%20arrow%20-%3E%20in%20name%20-%3E%20file%20with%20arrow\" 0 Sun%2C%2001%20Jan%202000%2000%3A00%3A00 SYMBOLIC-LINK \n");

  /* DOS style listing, space at the end of filename, year 1999 */
  rawData = "01-01-99  01:00AM                 1024 file \r\n";
  storeCache("ftp://localhost/bug365133/", rawData);
  parsedData = getURLContent("ftp://localhost/bug365133/");
  do_check_eq(parsedData,
    "300: ftp://localhost/bug365133/\n" +
    "200: filename content-length last-modified file-type\n" +
    "201: \"file%20\" 1024 Sun%2C%2001%20Jan%201999%2001%3A00%3A00 FILE \n");

  /* DOS style listing, tab at the end of filename, year 2000 */
  rawData = "01-01-00  01:00AM                 1024 file	\r\n";
  storeCache("ftp://localhost/bug365133/", rawData);
  parsedData = getURLContent("ftp://localhost/bug365133/");
  do_check_eq(parsedData,
    "300: ftp://localhost/bug365133/\n" +
    "200: filename content-length last-modified file-type\n" +
    "201: \"file%09\" 1024 Sun%2C%2001%20Jan%202000%2001%3A00%3A00 FILE \n");

}
