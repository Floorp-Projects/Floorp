const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://testing-common/httpd.js");

var httpserver = new HttpServer();
var testpath = "/simple";
var httpbody = "<?xml version='1.0' ?><root>0123456789</root>";

function createXHR(async)
{
  var xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
            .createInstance(Ci.nsIXMLHttpRequest);
  xhr.open("GET", "http://localhost:" +
           httpserver.identity.primaryPort + testpath, async);
  return xhr;
}

function checkResults(xhr)
{
  if (xhr.readyState != 4)
    return false;

  do_check_eq(xhr.status, 200);
  do_check_eq(xhr.responseText, httpbody);

  var root_node = xhr.responseXML.getElementsByTagName('root').item(0);
  do_check_eq(root_node.firstChild.data, "0123456789");
  return true;
}

function run_test()
{
  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.start(-1);

  // Test sync XHR sending
  var sync = createXHR(false);
  sync.send(null);
  checkResults(sync);

  // Test async XHR sending
  let async = createXHR(true);
  async.addEventListener("readystatechange", function(event) {
    if (checkResults(async))
      httpserver.stop(do_test_finished);
  }, false);
  async.send(null);
  do_test_pending();
}

function serverHandler(metadata, response)
{
  response.setHeader("Content-Type", "text/xml", false);
  response.bodyOutputStream.write(httpbody, httpbody.length);
}
