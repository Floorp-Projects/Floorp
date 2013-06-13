/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HTTP Version from request, used for response.
var gHttpVersion;

/* Debug and Error wrapper functions for dump().
 */
function ERR(response, responseCode, responseCodeStr, msg)
{
  // Dump to console log and send to client in response.
  dump("SERVER ERROR: " + msg + "\n");
  response.setStatusLine(gHttpVersion, responseCode, responseCodeStr);
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
  // Set http version for error responses.
  gHttpVersion = request.httpVersion;

  // All responses, inc. errors, are text/html.
  response.setHeader("Content-Type", "text/html; charset=UTF-8", false);

  // Get state var to determine if this is the first or second request.
  var expectedRequestType;
  if (getState("expectedRequestType") === "") {
    DBG("First call: Should be requesting full content.");
    expectedRequestType = "fullRequest";
    // Set state var for second request.
    setState("expectedRequestType", "partialRequest");
  } else if (getState("expectedRequestType") === "partialRequest") {
    DBG("Second call: Should be requesting undelivered content.");
    expectedRequestType = "partialRequest";
    // Reset state var for first request.
    setState("expectedRequestType", "");
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
  response.setHeader("ETag", "abcd0123", false);
  response.setHeader("Accept-Ranges", "bytes", false);

  // Prepare specific headers and content for first and second responses.
  if (expectedRequestType === "fullRequest") {
    DBG("First response: Sending partial content with a full header");
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(partialContent, partialContent.length);
    // Set Content-Length to full length of resource.
    response.setHeader("Content-Length", "" + totalLength, false);
  } else if (expectedRequestType === "partialRequest") {
    DBG("Second response: Sending remaining content with a range header");
    response.setStatusLine(request.httpVersion, 206, "Partial Content");
    response.setHeader("Content-Range", "bytes " + partialContent.length + "-" +
                       (totalLength - 1) + "/" + totalLength);
    response.write(remainderContent);
    // Set Content-Length to length of bytes transmitted.
    response.setHeader("Content-Length", "" + remainderContent.length, false);
  } else {
    // Somewhat redundant, but a check for errors in this test code.
    ERR(response, 500, "Internal Server Error",
       "Something very bad happened here: expectedRequestType is invalid " +
       "towards the end of handleRequest! - \"" + expectedRequestType + "\"");
    return;
  }
}
