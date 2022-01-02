function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 401, "Unauthorized");
  response.setHeader("WWW-Authenticate", 'Bearer realm="foo"');
}
