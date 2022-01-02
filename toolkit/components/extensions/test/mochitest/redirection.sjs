"use strict";

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 302);
  response.setHeader("Location", "./dummy_page.html");
}
