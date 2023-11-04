/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function handleRequest(request, response) {
  let params = new URLSearchParams(request.queryString);
  let uri = params.get("uri");
  let redirectType = params.get("redirectType") || "location";
  switch (redirectType) {
    case "refresh":
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Refresh", "0; url=" + uri);
      break;

    case "meta-refresh":
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "text/html");
      response.write(`<meta http-equiv="refresh" content="0; url=${uri}">`);
      break;

    case "location":
    // fall through
    default:
      response.setStatusLine(request.httpVersion, 302, "Moved Temporarily");
      response.setHeader("Location", uri);
  }
}
