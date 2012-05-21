/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);

  let action = "";
  let query = decodeURIComponent(request.queryString || "");
  response.write("<html xmlns=\"http://www.w3.org/1999/xhtml\"><head>");

  switch (query) {
    case "no_title":
      break;
    case "english_title":
      response.write("<title>English Title Page</title>");
      break;
    case "dynamic_title":
      response.write("<title>This is an english title page</title>");
      response.write("<script>window.addEventListener('load', function() { window.removeEventListener('load', arguments.callee, false); document.title = 'This is not a french title'; }, false);</script>");
      break;
    case "redirect":
      response.write("<meta http-equiv='refresh' content='1;url=http://mochi.test:8888/browser/mobile/chrome/tests/browser_title.sjs?no_title'></meta>");
      break;
    case "location":
      response.write("<script>window.addEventListener('load', function() { window.removeEventListener('load', arguments.callee, false); document.location = 'http://mochi.test:8888/browser/mobile/chrome/tests/browser_title.sjs?no_title' ; }, false);</script>");
      break;
    default:
      break;
  }
  response.write("</head><body>" + query + "</body></html>");
}
