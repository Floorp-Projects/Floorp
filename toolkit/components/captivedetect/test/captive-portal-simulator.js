/*
 * This is a NodeJS script that crudely simulates a captive portal.  It is
 * intended for use by QA and engineering while working on the captive portal
 * feature in Firefox.
 *
 * It maintains the authentication state (logged in or logged out). The root
 * URL ("/") displays either a link to log out (if the state is logged in) or
 * to log in (if the state is logged out).  A canonical URL ("/test") is
 * provided: when this URL is requested, if the state is logged in, a "success"
 * response is sent. If the state is logged out, a redirect is sent (back to
 * "/"). This script can be used to test Firefox's captive portal detection
 * features by setting the captivedetect.canonicalURL pref in about:config to
 * http://localhost:8080/test.
 *
 * Originally written by Nihanth.
 *
 * Run it like this:
 *
 *  mach node toolkit/components/captivedetect/test/captive-portal-simulator.js
 */

// eslint-disable-next-line no-undef
var http = require("http");

const PORT = 8080;

// Crude HTML for login and logout links
// loggedOutLink is displayed when the state is logged out, and shows a link
// that sets the state to logged in.
const loggedOutLink = "<html><body><a href='/set'>login</a></body></html>";
// loggedInLink is displayed when the state is logged in, and shows a link
// that resets the state to logged out.
const loggedInLink = "<html><body><a href='/reset'>logout</a></body></html>";

// Our state constants
const OUT = 0;
const IN = 1;

// State variable
var state = OUT;

function handleRequest(request, response) {
  if (request.url == "/reset") {
    // Set state to logged out and redirect to "/"
    state = OUT;
    response.writeHead(302, { location: "/" });
    response.end();
  } else if (request.url == "/set") {
    // Set state to logged in and redirect to canonical URL
    state = IN;
    response.writeHead(302, { location: "/test" });
    response.end();
  } else if (request.url == "/test") {
    // Canonical URL. Send canonical response if logged in, else
    // redirect to index.
    if (state == IN) {
      response.setHeader("Content-Type", "text/html");
      response.end(
        `<meta http-equiv="refresh" content="0;url=https://support.mozilla.org/kb/captive-portal"/>`
      );
      return;
    }
    response.writeHead(302, { location: "/" });
    response.end();
  } else {
    // Index: send a login or logout link based on state.
    response.setHeader("Content-Type", "text/html");
    response.end(state == IN ? loggedInLink : loggedOutLink);
  }
}

// Start the server.
var server = http.createServer(handleRequest);
server.listen(PORT, function () {
  console.log("Server listening on: http://localhost:%s", PORT);
});
