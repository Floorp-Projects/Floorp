/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This server-side script is used for browser_thumbnails_update.  One of the
// main things it must do in all cases is ensure a Cache-Control: no-store
// header, so the foreground capture doesn't interfere with the testing.

// If the querystring is "simple", then all it does it return some content -
// it doesn't really matter what that content is.

// Otherwise, its main role is that it must return different *content* for the
// second request than it did for the first.
// Also, it should be able to return an error response when requested for the
// second response.
// So the basic tests will be to grab the thumbnail, then request it to be
// grabbed again:
// * If the second request succeeded, the new thumbnail should exist.
// * If the second request is an error, the new thumbnail should be ignored.

function handleRequest(aRequest, aResponse) {
  aResponse.setHeader("Content-Type", "text/html;charset=utf-8", false);
  // we want to disable gBrowserThumbnails on-load capture for these responses,
  // so set as a "no-store" response.
  aResponse.setHeader("Cache-Control", "no-store");

  // for the simple test - just return some content.
  if (aRequest.queryString == "simple") {
    aResponse.write("<html><body></body></html>");
    aResponse.setStatusLine(aRequest.httpVersion, 200, "Its simply OK");
    return;
  }

  // it's one of the more complex tests where the first request for the given
  // URL must return different content than the second, and possibly an error
  // response for the second
  let doneError = getState(aRequest.queryString);
  if (!doneError) {
    // first request - return a response with a green body and 200 response.
    aResponse.setStatusLine(aRequest.httpVersion, 200, "OK - It's green");
    aResponse.write("<html><body bgcolor=00ff00></body></html>");
    // set the  state so the next request does the "second request" thing below.
    setState(aRequest.queryString, "yep");
  } else {
    // second request - this will be done by the b/g service.
    // We always return a red background, but depending on the query string we
    // return either a 200 or 401 response.
    if (aRequest.queryString == "fail")
      aResponse.setStatusLine(aRequest.httpVersion, 401, "Oh no you don't");
    else
      aResponse.setStatusLine(aRequest.httpVersion, 200, "OK - It's red");
    aResponse.write("<html><body bgcolor=ff0000></body></html>");
    // reset the error state incase this ends up being reused for the
    // same url and querystring.
    setState(aRequest.queryString, "");
  }
}
