const URL = "ftp://localhost/bug365133/";

const tests = [
  [ /* Unix style listing, space at the end of filename */
    "drwxrwxr-x    2 500      500          4096 Jan 01  2000 a \r\n"
  ,
    "300: " + URL + "\n" +
    "200: filename content-length last-modified file-type\n" +
    "201: \"a%20\" 0 Sun%2C%2001%20Jan%202000%2000%3A00%3A00 DIRECTORY \n"
  ],
  [ /* Unix style listing, space at the end of link name */
    "lrwxrwxrwx    1 500      500             2 Jan 01  2000 l  -> a \r\n"
  ,
    "300: " + URL + "\n" +
    "200: filename content-length last-modified file-type\n" +
    "201: \"l%20\" 2 Sun%2C%2001%20Jan%202000%2000%3A00%3A00 SYMBOLIC-LINK \n"
  ],
  [ /* Unix style listing, regular file with " -> " in name */
    "-rw-rw-r--    1 500      500             0 Jan 01  2000 arrow -> in name \r\n"
  ,
    "300: " + URL + "\n" +
    "200: filename content-length last-modified file-type\n" +
    "201: \"arrow%20-%3E%20in%20name%20\" 0 Sun%2C%2001%20Jan%202000%2000%3A00%3A00 FILE \n"
  ],
  [ /* Unix style listing, tab at the end of filename */
    "drwxrwxrwx    2 500      500          4096 Jan 01  2000 t	\r\n"
  ,
    "300: " + URL + "\n" +
    "200: filename content-length last-modified file-type\n" +
    "201: \"t%09\" 0 Sun%2C%2001%20Jan%202000%2000%3A00%3A00 DIRECTORY \n"
  ],
  [ /* Unix style listing, multiple " -> " in filename */
    "lrwxrwxrwx    1 500      500            26 Jan 01  2000 symlink with arrow -> in name -> file with arrow -> in name\r\n"
  ,
    "300: " + URL + "\n" +
    "200: filename content-length last-modified file-type\n" +
    "201: \"symlink%20with%20arrow%20-%3E%20in%20name\" 26 Sun%2C%2001%20Jan%202000%2000%3A00%3A00 SYMBOLIC-LINK \n"
  ],
  [ /* Unix style listing, multiple " -> " in filename, incorrect filesize */
    "lrwxrwxrwx    1 500      500             0 Jan 01  2000 symlink with arrow -> in name -> file with arrow -> in name\r\n"
  ,
    "300: " + URL + "\n" +
    "200: filename content-length last-modified file-type\n" +
    "201: \"symlink%20with%20arrow%20-%3E%20in%20name%20-%3E%20file%20with%20arrow\" 0 Sun%2C%2001%20Jan%202000%2000%3A00%3A00 SYMBOLIC-LINK \n"
  ],
  [ /* DOS style listing, space at the end of filename, year 1999 */
    "01-01-99  01:00AM                 1024 file \r\n"
  ,
    "300: " + URL + "\n" +
    "200: filename content-length last-modified file-type\n" +
    "201: \"file%20\" 1024 Sun%2C%2001%20Jan%201999%2001%3A00%3A00 FILE \n"
  ],
  [ /* DOS style listing, tab at the end of filename, year 2000 */
    "01-01-00  01:00AM                 1024 file	\r\n"
  ,
    "300: " + URL + "\n" +
    "200: filename content-length last-modified file-type\n" +
    "201: \"file%09\" 1024 Sun%2C%2001%20Jan%202000%2001%3A00%3A00 FILE \n"
  ]
]

function checkData(request, data, ctx) {
  do_check_eq(tests[0][1], data);
  tests.shift();
  do_execute_soon(next_test);
}

function storeData() {
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
