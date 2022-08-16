function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200);
  // Write the cookie header value to the body for the test to inspect.
  response.write(request.getHeader("Cookie"));
}
