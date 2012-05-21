/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");

  if (!request.queryString.match(/^name=/))
    return;
  var name = decodeURIComponent(request.queryString.substring(5));

  response.setHeader("Content-Type", "application/octet-stream; name=\"" + name + "\"");
  response.setHeader("Content-Disposition", "inline; filename=\"" + name + "\"");
}
