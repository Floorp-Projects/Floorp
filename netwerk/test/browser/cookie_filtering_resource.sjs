/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);

  // configure set-cookie domain
  let domain = "";
  if (request.hasHeader("return-cookie-domain")) {
    domain = "; Domain=" + request.getHeader("return-cookie-domain");
  }

  // configure set-cookie sameSite
  let authStr = "; Secure";
  if (request.hasHeader("return-insecure-cookie")) {
    authStr = "";
  }

  // use headers to decide if we have them
  if (request.hasHeader("return-set-cookie")) {
    response.setHeader(
      "Set-Cookie",
      request.getHeader("return-set-cookie") + authStr + domain,
      false
    );
  }

  let body = "<!DOCTYPE html> <html> <body> true </body> </html>";
  response.write(body);
}
