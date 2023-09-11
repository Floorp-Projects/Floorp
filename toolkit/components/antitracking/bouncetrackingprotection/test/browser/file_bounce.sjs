function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);

  Cu.importGlobalProperties(["URLSearchParams"]);
  let query = new URLSearchParams(request.queryString);

  if (query.get("setCookie")) {
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
