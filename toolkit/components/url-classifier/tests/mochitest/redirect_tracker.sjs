const gURL =
  "http://trackertest.org/tests/toolkit/components/url-classifier/tests/mochitest/evil.js";

function handleRequest(request, response) {
  response.setStatusLine("1.1", 302, "found");
  response.setHeader("Location", gURL, false);
}
