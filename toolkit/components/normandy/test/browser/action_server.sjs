// Returns JS for an action, regardless of the URL.
function handleRequest(request, response) {
  // Allow cross-origin, so you can XHR to it!
  response.setHeader("Access-Control-Allow-Origin", "*", false);
  // Avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  // Write response body
  response.write('registerAsyncCallback("action", async () => {});');
}
