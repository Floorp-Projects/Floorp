"use strict";

function handleRequest(request, response) {
  const query = new URLSearchParams(request.queryString);

  // If the
  let type = "text/html";
  if (query.has("type")) {
    type = query.get("type");
  }
  response.setHeader("Content-Type", type, false);

  response.write(`<h1>hello</h1>`);
}
