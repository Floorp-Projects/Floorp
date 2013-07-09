/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Debug and Error wrapper functions for dump().
 */
function ERR(response, responseCode, responseCodeStr, msg)
{
  // Reset state var.
  setState("expectedRequestType", "");
  // Dump to console log and send to client in response.
  dump("SERVER ERROR: " + msg + "\n");
  response.write("HTTP/1.1" + responseCode + responseCodeStr + "\r\n");
  response.write("Content-Type: text/html; charset=UTF-8\r\n");
  response.write("Content-Length: " + msg.length + "\r\n");
  response.write("\r\n");
  response.write(msg);
}

function DBG(msg)
{
  // Dump to console only.
  dump("SERVER DEBUG: " + msg + "\n");
}

/* Delivers content in parts to test partially cached content: requires two
 * requests for the same resource.
 *
 * First call will respond with partial content, but a 200 header and
 * Content-Length equal to the full content length. No Range or If-Range
 * headers are allowed in the request.
 *
 * Second call will require Range and If-Range in the request headers, and
 * will respond with the range requested.
 */
function handleRequest(request, response)
{
  DBG("Trying to seize power");
  response.seizePower();

  DBG("About to check state vars");
  // Get state var to determine if this is the first or second request.
  var expectedRequestType;
  var lastModified;
  if (getState("expectedRequestType") === "") {
    DBG("First call: Should be requesting full content.");
    expectedRequestType = "fullRequest";
    // Set state var for second request.
    setState("expectedRequestType", "partialRequest");
    // Create lastModified variable for responses.
    lastModified = (new Date()).toUTCString();
    setState("lastModified", lastModified);
  } else if (getState("expectedRequestType") === "partialRequest") {
    DBG("Second call: Should be requesting undelivered content.");
    expectedRequestType = "partialRequest";
    // Reset state var for first request.
    setState("expectedRequestType", "");
    // Get last modified date and reset state var.
    lastModified = getState("lastModified");
  } else {
    ERR(response, 500, "Internal Server Error",
        "Invalid expectedRequestType \"" + expectedRequestType + "\"in " +
        "server state db.");
    return;
  }

  // Look for Range and If-Range
  var range = request.hasHeader("Range") ? request.getHeader("Range") : "";
  var ifRange = request.hasHeader("If-Range") ? request.getHeader("If-Range") : "";

  if (expectedRequestType === "fullRequest") {
    // Should not have Range or If-Range in first request.
    if (range && range.length > 0) {
      ERR(response, 400, "Bad Request",
          "Should not receive \"Range: " + range + "\" for first, full request.");
      return;
    }
    if (ifRange && ifRange.length > 0) {
      ERR(response, 400, "Bad Request",
          "Should not receive \"Range: " + range + "\" for first, full request.");
      return;
    }
  } else if (expectedRequestType === "partialRequest") {
    // Range AND If-Range should both be present in second request.
    if (!range) {
      ERR(response, 400, "Bad Request",
          "Should receive \"Range: \" for second, partial request.");
      return;
    }
    if (!ifRange) {
      ERR(response, 400, "Bad Request",
          "Should receive \"If-Range: \" for second, partial request.");
      return;
    }
  } else {
    // Somewhat redundant, but a check for errors in this test code.
    ERR(response, 500, "Internal Server Error",
        "expectedRequestType not set correctly: \"" + expectedRequestType + "\"");
    return;
  }

  // Prepare content in two parts for responses.
  var partialContent = "<html><head></head><body><p id=\"firstResponse\">" +
                       "First response</p>";
  var remainderContent = "<p id=\"secondResponse\">Second response</p>" +
                         "</body></html>";
  var totalLength = partialContent.length + remainderContent.length;

  DBG("totalLength: " + totalLength);

  // Prepare common headers for the two responses.
  date = new Date();
  DBG("Date: " + date.toUTCString() + ", Last-Modified: " + lastModified);
  var commonHeaders = "Date: " + date.toUTCString() + "\r\n" +
                      "Last-Modified: " + lastModified + "\r\n" +
                      "Content-Type: text/html; charset=UTF-8\r\n" +
                      "ETag: abcd0123\r\n" +
                      "Accept-Ranges: bytes\r\n";


  // Prepare specific headers and content for first and second responses.
  if (expectedRequestType === "fullRequest") {
    DBG("First response: Sending partial content with a full header");
    response.write("HTTP/1.1 200 OK\r\n");
    response.write(commonHeaders);
    // Set Content-Length to full length of resource.
    response.write("Content-Length: " + totalLength + "\r\n");
    response.write("\r\n");
    response.write(partialContent);
  } else if (expectedRequestType === "partialRequest") {
    DBG("Second response: Sending remaining content with a range header");
    response.write("HTTP/1.1 206 Partial Content\r\n");
    response.write(commonHeaders);
    // Set Content-Length to length of bytes transmitted.
    response.write("Content-Length: " + remainderContent.length + "\r\n");
    response.write("Content-Range: bytes " + partialContent.length + "-" +
                   (totalLength - 1) + "/" + totalLength + "\r\n");
    response.write("\r\n");
    response.write(remainderContent);
  } else {
    // Somewhat redundant, but a check for errors in this test code.
    ERR(response, 500, "Internal Server Error",
       "Something very bad happened here: expectedRequestType is invalid " +
       "towards the end of handleRequest! - \"" + expectedRequestType + "\"");
    return;
  }

  response.finish();
}
