/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript sts=2 sw=2 et tw=80: */
"use strict";

/* exported handleRequest */

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/json", false);
  response.setHeader("Access-Control-Allow-Credentials", "true", false);
  response.setHeader("Access-Control-Allow-Origin", "*", false);


  let headers = {};
  // Why on earth is this a nsISimpleEnumerator...
  let enumerator = request.headers;
  while (enumerator.hasMoreElements()) {
    let header = enumerator.getNext().data;
    headers[header.toLowerCase()] = request.getHeader(header);
  }

  response.write(JSON.stringify(headers));
}

