
do_load_httpd_js();

var httpserver = new nsHttpServer();
var testpath = "/simple";
var httpbody = "<?xml version='1.0' ?><root>0123456789</root>";

function run_test()
{
  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.start(4444);

  var xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
              .createInstance(Ci.nsIXMLHttpRequest);
  xhr.open("GET", "http://localhost:4444" + testpath, true);
  xhr.onreadystatechange = function(event) {
    if (xhr.readyState == 4) {
      do_check_eq(xhr.status, 200);
      do_check_eq(xhr.responseText, httpbody);

      var root_node = xhr.responseXML.getElementsByTagName('root').item(0);
      do_check_eq(root_node.firstChild.data, "0123456789");

      httpserver.stop(do_test_finished);
    }
  }
  xhr.send(null);

  do_test_pending();
}

function serverHandler(metadata, response)
{
  response.setHeader("Content-Type", "text/xml", false);
  response.bodyOutputStream.write(httpbody, httpbody.length);
}
