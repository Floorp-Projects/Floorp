"use strict";

// usage via url parameters:
//  - link: if set sends a link header with the given link value as an early hint repsonse
//  - location: sets destination of 301 response

function handleRequest(request, response) {
  let qs = new URLSearchParams(request.queryString);
  let link = qs.get("link");
  let location = qs.get("location");

  // write to raw socket
  response.seizePower();
  if (link != undefined) {
    response.write("HTTP/1.1 103 Early Hint\r\n");
    response.write(`Link: ${link}\r\n`);
    response.write("\r\n");
  }

  response.write("HTTP/1.1 307 Temporary Redirect\r\n");
  response.write(`Location: ${location}\r\n`);
  response.write("\r\n");
  response.finish();
}
