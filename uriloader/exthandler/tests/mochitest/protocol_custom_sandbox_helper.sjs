/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  let query = new URLSearchParams(request.queryString);

  // Set CSP sandbox attributes if caller requests any.
  let cspSandbox = query.get("cspSandbox");
  if (cspSandbox) {
    response.setHeader(
      "Content-Security-Policy",
      "sandbox " + cspSandbox,
      false
    );
  }

  // Redirect to custom protocol via HTTP 302.
  if (query.get("redirectCustomProtocol")) {
    response.setStatusLine(request.httpVersion, 302, "Found");

    response.setHeader("Location", "mailto:test@example.com", false);
    response.write("Redirect!");
    return;
  }

  response.setStatusLine(request.httpVersion, 200);
  response.write("OK");
}
