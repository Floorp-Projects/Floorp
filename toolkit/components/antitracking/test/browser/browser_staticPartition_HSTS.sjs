/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAA" +
  "DUlEQVQImWNgY2P7DwABOgESJhRQtgAAAABJRU5ErkJggg==");

const PAGE = "<!DOCTYPE html><html><body><p>HSTS page</p></body></html>";

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, "200", "OK");
  if (request.queryString == "includeSub") {
    response.setHeader("Strict-Transport-Security", "max-age=60; includeSubDomains");
  } else {
    response.setHeader("Strict-Transport-Security", "max-age=60");
  }

  if (request.queryString == "image") {
    response.setHeader("Content-Type", "image/png", false);
    response.setHeader("Content-Length", IMG_BYTES.length + "", false);
    response.write(IMG_BYTES);
    return;
  }

  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Content-Length", PAGE.length + "", false);
  response.write(PAGE);
}
