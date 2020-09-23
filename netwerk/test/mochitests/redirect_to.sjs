function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 308, "Permanent Redirect");
  response.setHeader("Location", request.queryString);
}
