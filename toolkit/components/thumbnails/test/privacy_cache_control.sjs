/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function handleRequest(aRequest, aResponse) {
  // Set HTTP Status
  aResponse.setStatusLine(aRequest.httpVersion, 200, "OK");

  // Set Cache-Control header.
  let value = aRequest.queryString;
  if (value)
    aResponse.setHeader("Cache-Control", value);

  // Set the response body.
  aResponse.write("<!DOCTYPE html><html><body></body></html>");
}
