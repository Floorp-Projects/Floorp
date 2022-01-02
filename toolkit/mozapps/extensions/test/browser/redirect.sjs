function handleRequest(request, response) {
  dump("*** Received redirect for " + request.queryString + "\n");
  response.setStatusLine(request.httpVersion, 301, "Moved Permanently");
  response.setHeader("Location", request.queryString, false);
}
