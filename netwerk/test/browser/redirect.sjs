function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 301, "Moved Permanently");
  let location = request.queryString;
  response.setHeader("Location", location, false);
  response.write("Hello world!");
}
