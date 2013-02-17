/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(aRequest, aResponse) {
 // Set HTTP Status.
 aResponse.setStatusLine(aRequest.httpVersion, 301, "Moved Permanently");

 // Set redirect URI.
 aResponse.setHeader("Location", "http://mochi.test:8888/browser/toolkit/components/thumbnails/test/background_red.html");
}
