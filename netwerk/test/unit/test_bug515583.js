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
   "201: \"A\" 2048 Wed%2C%2003%20Mar%201993%2018%3A09%3A01 FILE \n" +
   "201: \"E\" 2048 Mon%2C%2008%20Mar%201993%2018%3A09%3A01 FILE \n"]
   ,
   ["\r\r\r\n"
   ,
   "300: " + URL + "\n" +
   "200: filename content-length last-modified file-type\n"]
]

function checkData(request, data, ctx) {
  do_check_eq(tests[0][1], data);
  tests.shift();
  do_execute_soon(next_test);
}

function storeData(status, entry) {
  var scs = Cc["@mozilla.org/streamConverters;1"].
            getService(Ci.nsIStreamConverterService);
  var converter = scs.asyncConvertData("text/ftp-dir", "application/http-index-format",
                                       new ChannelListener(checkData, null, CL_ALLOW_UNKNOWN_CL), null);

  var stream = Cc["@mozilla.org/io/string-input-stream;1"].
               createInstance(Ci.nsIStringInputStream);
  stream.data = tests[0][0];

  var url = {
    password: "",
    asciiSpec: URL,
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIURI])
  };

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
  do_execute_soon(next_test);
  do_test_pending();
}
