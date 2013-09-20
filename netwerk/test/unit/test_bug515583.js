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
                        "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
                        storeData);
  }
}

function run_test() {
  do_execute_soon(next_test);
  do_test_pending();
}
