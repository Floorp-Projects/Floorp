var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 301, "Moved Permanently");
  response.setHeader("Location", "empty.html#");
}
