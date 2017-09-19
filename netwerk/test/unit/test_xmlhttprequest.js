
ChromeUtils.import("resource://testing-common/httpd.js");

Cu.importGlobalProperties(["XMLHttpRequest"]);

var httpserver = new HttpServer();
var testpath = "/simple";
var httpbody = "<?xml version='1.0' ?><root>0123456789</root>";

function createXHR(async)
{
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "http://localhost:" +
           httpserver.identity.primaryPort + testpath, async);
  return xhr;
}

function checkResults(xhr)
{
  if (xhr.readyState != 4)
    return false;

  Assert.equal(xhr.status, 200);
  Assert.equal(xhr.responseText, httpbody);

  var root_node = xhr.responseXML.getElementsByTagName('root').item(0);
  Assert.equal(root_node.firstChild.data, "0123456789");
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
  });
  async.send(null);
  do_test_pending();
}

function serverHandler(metadata, response)
{
  response.setHeader("Content-Type", "text/xml", false);
  response.bodyOutputStream.write(httpbody, httpbody.length);
}
