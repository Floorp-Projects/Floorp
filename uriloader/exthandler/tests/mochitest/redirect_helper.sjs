/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.importGlobalProperties(["URLSearchParams"]);

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 302, "Moved Temporarily");
  response.setHeader(
    "Location",
    new URLSearchParams(request.queryString).get("uri")
  );
}
