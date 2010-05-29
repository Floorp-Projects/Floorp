// Simple script redirects to the query part of the uri if the browser
// authenticates with username "testuser" password "testpass"

function handleRequest(request, response) {
  if (request.hasHeader("Authorization")) {
    if (request.getHeader("Authorization") == "Basic dGVzdHVzZXI6dGVzdHBhc3M=") {
      response.setStatusLine(request.httpVersion, 302, "Found");
      response.setHeader("Location", request.queryString);
      response.write("See " + request.queryString);
    }
    else {
      response.setStatusLine(request.httpVersion, 403, "Forbidden");
      response.write("Invalid credentials");
    }
  }
  else {
    response.setStatusLine(request.httpVersion, 401, "Authentication required");
    response.setHeader("WWW-Authenticate", "basic realm=\"XPInstall\"", false);
    response.write("Unauthenticated request");
  }
}
