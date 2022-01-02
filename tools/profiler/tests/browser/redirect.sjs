function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 301, "Moved Permanently");
  response.setHeader(
    "Location",
    decodeURIComponent(request.queryString),
    false
  );
}
