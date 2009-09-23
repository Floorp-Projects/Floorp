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

const URL = "ftp://localhost/bug515583/";

const tests = [
  ["[RWCEM1 4 1-MAR-1993 18:09:01.12\r\n" +
   "[RWCEM1] 4 2-MAR-1993 18:09:01.12\r\n" +
   "[RWCEM1]A 4 3-MAR-1993 18:09:01.12\r\n" +
   "[RWCEM1]B; 4 4-MAR-1993 18:09:01.12\r\n" +
   "[RWCEM1];1 4 5-MAR-1993 18:09:01.12\r\n" +
   "[RWCEM1]; 4 6-MAR-1993 18:09:01.12\r\n" +
   "[RWCEM1]C;1D 4 7-MAR-1993 18:09:01.12\r\n" +
   "[RWCEM1]E;1 4 8-MAR-1993 18:09:01.12\r\n"
  ,
   "300: " + URL + "\n" +
   "200: filename content-length last-modified file-type\n" +
   "201: \"A\" 2048 Sun%2C%2003%20Mar%201993%2018%3A09%3A01 FILE \n" +
   "201: \"E\" 2048 Sun%2C%2008%20Mar%201993%2018%3A09%3A01 FILE \n"]
   ,
   ["\r\r\r\n"
   ,
   "300: " + URL + "\n" +
   "200: filename content-length last-modified file-type\n"]
]

function run_test() {
  for (var i = 0; i < tests.length; i++) {
    storeCache(URL, tests[i][0]);
    var parsedData = getURLContent(URL);
    do_check_eq(parsedData, tests[i][1]);
  }
}
