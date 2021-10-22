/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  let hstsHeader = "max-age=300";
  if (request.queryString == "includeSubdomains") {
    hstsHeader += "; includeSubdomains";
  }
  response.setHeader("Strict-Transport-Security", hstsHeader);
  response.setHeader("Pragma", "no-cache");
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);
  response.setStatusLine(request.httpVersion, 200);
  response.write("<!DOCTYPE html><html><body><h1>Ok!</h1></body></html>");
}
