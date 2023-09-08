/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Redirect a request for an icon to a different place, using a different
 * content-type.
 */

function handleRequest(request, response) {
  response.setStatusLine("1.1", 302, "Moved");
  if (request.queryString == "type=invalid") {
    response.setHeader("Content-Type", "image/png", false);
    response.setHeader("Location", "/head_search.js", false);
  } else {
    response.setHeader("Content-Type", "text/html", false);
    response.setHeader("Location", "remoteIcon.ico", false);
  }
}
