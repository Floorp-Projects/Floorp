/**
 * Reads an HTTP status code and response body from the querystring and sends
 * back a matching response.
 */
function handleRequest(request, response) {
  // Allow cross-origin, so you can XHR to it!
  response.setHeader("Access-Control-Allow-Origin", "*", false);
  // Avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  const params = request.queryString.split("&");
  for (const param of params) {
    const [key, value] = param.split("=");
    if (key === "status") {
      response.setStatusLine(null, value);
    } else if (key === "body") {
      response.write(value);
    }
  }
  response.write("");
}
