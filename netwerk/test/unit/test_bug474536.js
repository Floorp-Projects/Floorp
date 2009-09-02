
do_load_httpd_js();

var httpserv = null;

var _CSvc = null;
function get_cache_service() 
{
  if (_CSvc)
    return _CSvc;

  return _CSvc = Cc["@mozilla.org/network/cache-service;1"].
                 getService(Ci.nsICacheService);
}

function setup_server()
{
  httpserv = new nsHttpServer();
  var dataDir = do_get_file("data/bug474536/");
  httpserv.registerDirectory("/", dataDir);
  httpserv.start(4444);
}

function makeJARChannel(url)
{
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  return ios.newChannel(url, null, null).QueryInterface(Ci.nsIJARChannel);
}

var testNumber = 1;

var listener = {
  // In all test cases, the Content-disposition header returned during the HTTP
  // request should be visible in the Channel (HTTP, JAR, or nested JAR)
  onStartRequest: function test_onStartR(request, ctx) {
    try {
      do_check_true(request instanceof Ci.nsIPropertyBag);
      try {
        dispo = request.getProperty("content-disposition"); 
      } catch (e) {
        do_throw("content-disposition not present!");
      }
      do_check_eq(request.getProperty("content-disposition"),
                                      "attachment; filename=foo.jar");
    } catch (e) {
      do_throw("unexpected exception: " + e);
    }
    throw Components.results.NS_ERROR_ABORT;
  },
  onDataAvailable: function(request, context, stream, offset, count) {
    throw Components.results.NS_ERROR_UNEXPECTED; 
  },
  onStopRequest: function test_onStopR(request, ctx, status) {
    switch (testNumber) {
      case 1:
        run_test2_JAR_JAR_HTTP();
        break;
      case 2:
        httpserv.stop();
        break;
      default:
        do_throw("Inconceivable!");
    }
    testNumber++;
    do_test_finished();
  }
};

function run_test()
{
  setup_server(); 

  test1_JAR_HTTP();
}

function test1_JAR_HTTP() {
  var jarChannel1 = 
    makeJARChannel("jar:http://localhost:4444/foo.jar!/foo.html");
  jarChannel1.asyncOpen(listener, null);
  do_test_pending();
}

function run_test2_JAR_JAR_HTTP()
{
  var jarChannel1 = 
    makeJARChannel("jar:jar:http://localhost:4444/foo.jar!/bar.jar!/bar.html");
  jarChannel1.asyncOpen(listener, null);
  do_test_pending();
}

