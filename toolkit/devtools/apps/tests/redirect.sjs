const REDIRECTION_URL = "http://example.com/redirection-target.html";

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 301, "Moved Permanently");
  response.setHeader("Location", REDIRECTION_URL, false);
}

