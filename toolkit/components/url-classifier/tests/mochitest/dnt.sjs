function handleRequest(request, response) {
  var dnt = "unspecified";
  if (request.hasHeader("DNT")) {
    dnt = "1";
  }

  response.setHeader("Content-Type", "text/plain", false);
  response.write(dnt);
}
