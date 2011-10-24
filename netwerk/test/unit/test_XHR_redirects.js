// This file tests whether XmlHttpRequests correctly handle redirects,
// including rewriting POSTs to GETs (on 301/302/303), as well as
// prompting for redirects of other unsafe methods (such as PUTs, DELETEs,
// etc--see HttpBaseChannel::IsSafeMethod).  Since no prompting is possible
// in xpcshell, we get an error for prompts, and the request fails.

do_load_httpd_js();

var server;
const BUGID = "676059";

function createXHR(async, method, path)
{
  var xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
            .createInstance(Ci.nsIXMLHttpRequest);
  xhr.open(method, "http://localhost:4444" + path, async);
  return xhr;
}

function checkResults(xhr, method, status)
{
  if (xhr.readyState != 4)
    return false;

  do_check_eq(xhr.status, status);
  
  if (status == 200) {
    // if followed then check for echoed method name
    do_check_eq(xhr.getResponseHeader("X-Received-Method"), method);
  }
  
  return true;
}

function run_test() {
  // start server
  server = new nsHttpServer();

  server.registerPathHandler("/bug" + BUGID + "-redirect301", bug676059redirect301);
  server.registerPathHandler("/bug" + BUGID + "-redirect302", bug676059redirect302);
  server.registerPathHandler("/bug" + BUGID + "-redirect303", bug676059redirect303);
  server.registerPathHandler("/bug" + BUGID + "-redirect307", bug676059redirect307);
  server.registerPathHandler("/bug" + BUGID + "-target", bug676059target);

  server.start(4444);

  // format: redirectType, methodToSend, redirectedMethod, finalStatus
  //   redirectType sets the URI the initial request goes to
  //   methodToSend is the HTTP method to send
  //   redirectedMethod is the method to use for the redirect, if any
  //   finalStatus is 200 when the redirect takes place, redirectType otherwise
  
  // Note that unsafe methods should not follow the redirect automatically
  // Of the methods below, DELETE, POST and PUT are unsafe
    
  var tests = [
    // 301: rewrite just POST
    [301, "DELETE", "GET", 200], // but see bug 598304
    [301, "GET", "GET", 200],
    [301, "HEAD", "GET", 200], // but see bug 598304
    [301, "POST", "GET", 200],
    [301, "PUT", "GET", 200], // but see bug 598304
    [301, "PROPFIND", "GET", 200], // but see bug 598304
    // 302: see 301
    [302, "DELETE", "GET", 200], // but see bug 598304
    [302, "GET", "GET", 200],
    [302, "HEAD", "GET", 200], // but see bug 598304
    [302, "POST", "GET", 200],
    [302, "PUT", "GET", 200], // but see bug 598304
    [302, "PROPFIND", "GET", 200], // but see bug 598304
    // 303: rewrite to GET except HEAD
    [303, "DELETE", "GET", 200],
    [303, "GET", "GET", 200],
    [303, "HEAD", "GET", 200],
    [303, "POST", "GET", 200],
    [303, "PUT", "GET", 200],
    [303, "PROPFIND", "GET", 200],
    // 307: never rewrite
    [307, "DELETE", null, 307],
    [307, "GET", "GET", 200],
    [307, "HEAD", "HEAD", 200],
    [307, "POST", null, 307],
    [307, "PUT", null, 307],
    [307, "PROPFIND", "PROPFIND", 200],
  ];

  var xhr;
  
  for (var i = 0; i < tests.length; ++i) {
    dump("Testing " + tests[i] + "\n");
    xhr = createXHR(false, tests[i][1], "/bug" + BUGID + "-redirect" + tests[i][0]);
    xhr.send(null);
    checkResults(xhr, tests[i][2], tests[i][3]);
  }  

  server.stop(do_test_finished);
}
 
 // PATH HANDLER FOR /bug676059-redirect301
function bug676059redirect301(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 301, "Moved Permanently");
  response.setHeader("Location", "http://localhost:4444/bug" + BUGID + "-target");
}

 // PATH HANDLER FOR /bug676059-redirect302
function bug676059redirect302(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 302, "Found");
  response.setHeader("Location", "http://localhost:4444/bug" + BUGID + "-target");
}

 // PATH HANDLER FOR /bug676059-redirect303
function bug676059redirect303(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 303, "See Other");
  response.setHeader("Location", "http://localhost:4444/bug" + BUGID + "-target");
}

 // PATH HANDLER FOR /bug676059-redirect307
function bug676059redirect307(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 307, "Temporary Redirect");
  response.setHeader("Location", "http://localhost:4444/bug" + BUGID + "-target");
}

 // PATH HANDLER FOR /bug676059-target
function bug676059target(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("X-Received-Method", metadata.method);
}