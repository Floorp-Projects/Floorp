/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);

  let action = "";
  let query = decodeURIComponent(request.queryString || "");
  response.write("<html xmlns=\"http://www.w3.org/1999/xhtml\"><head>");
  response.write("<meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1, minimum-scale=1'>");

  let body = "<span>" + query + "</span>";
  switch (query) {
    case "blank":
      response.write("<title>This is a not a scrollable page</title>");
      break;
    case "horizontal":
      response.write("<title>This is a horizontally scrollable page</title>");
      body += "\n<div style='height: 200px; width: 2000px; border: 1px solid red; background-color: grey; color: black'>Browser scrollbar test</div>";
      break;
    case "vertical":
      response.write("<title>This is a vertically scrollable page</title>");
      body += "\n<div style='height: 2000px; width: 200px; border: 1px solid red; background-color: grey; color: black'>Browser scrollbar test</div>";
      break;
    case "both":
      response.write("<title>This is a scrollable page in both directions</title>");
      body += "\n<div style='height: 2000px; width: 2000px; border: 1px solid red; background-color: grey; color: black'>Browser scrollbar test</div>";
      break;
    default:
      break;
  }
  response.write("</head><body style='border:2px solid blue'>" + body + "</body></html>");
}
