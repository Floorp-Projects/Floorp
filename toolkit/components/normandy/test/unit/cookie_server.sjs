/**
 * Sends responses that sets a cookie.
 */
function handleRequest(request, response) {
  // Allow cross-origin, so you can XHR to it!
  response.setHeader("Access-Control-Allow-Origin", "*", false);
  // Avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  // Set a cookie
  response.setHeader("Set-Cookie", "type=chocolate-chip", false);
  response.write("");
}
