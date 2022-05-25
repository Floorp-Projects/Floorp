function handleRequest(request, response) {
  response.setHeader(
    "Strict-Transport-Security",
    "max-age=60; includeSubDomains"
  );
}
