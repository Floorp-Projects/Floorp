/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function handleRequest(request, response) {
  response.setStatusLine("1.1", 301, "Found");
  response.setHeader("Location", "redirect_once.sjs", false);
}
