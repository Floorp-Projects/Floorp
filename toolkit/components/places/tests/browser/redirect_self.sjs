/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Script that redirects to itself once.

function handleRequest(request, response) {
  if (
    request.hasHeader("Cookie") &&
    request.getHeader("Cookie").includes("redirect-self")
  ) {
    response.setStatusLine("1.0", 200, "OK");
    // Expire the cookie.
    response.setHeader(
      "Set-Cookie",
      "redirect-self=true; expires=Thu, 01 Jan 1970 00:00:00 GMT",
      true
    );
    response.setHeader("Content-Type", "text/plain; charset=utf-8", false);
    response.write("OK");
  } else {
    response.setStatusLine(request.httpVersion, 302, "Moved Temporarily");
    response.setHeader("Set-Cookie", "redirect-self=true", true);
    response.setHeader("Location", "redirect_self.sjs");
    response.write("Moved Temporarily");
  }
}
