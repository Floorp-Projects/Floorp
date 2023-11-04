function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);

  let query = new URLSearchParams(request.queryString);

  let setState = query.get("setState");
  if (setState == "cookie-server") {
    response.setHeader("Set-Cookie", "foo=bar");
  }

  let statusCode = 302;
  let statusCodeQuery = query.get("statusCode");
  if (statusCodeQuery) {
    statusCode = Number.parseInt(statusCodeQuery);
  }

  response.setStatusLine("1.1", statusCode, "Found");
  response.setHeader("Location", query.get("target"), false);
}
