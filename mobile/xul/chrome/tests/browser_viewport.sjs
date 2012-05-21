/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 function decodeQuery(query) {
   let result = {};
   query.split("&").forEach(function(pair) {
     let [key, val] = pair.split("=");
     result[key] = decodeURIComponent(val);
   });
   return result;
 }

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);

  let params = decodeQuery(request.queryString || "");

  if (params.xhtml == "true") {
    response.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    response.write("<!DOCTYPE html PUBLIC \"-//WAPFORUM//DTD XHTML Mobile 1.0//EN\" \"http://www.wapforum.org/DTD/xhtml-mobile10.dtd\">");
  }
  response.write("<html xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>Browser Viewport Test</title>");
  if (params.metadata)
    response.write("<meta name=\"viewport\" content=\"" + params.metadata + "\"/>");
  response.write("</head><body");

  if (params.style)
    response.write(" style=\"" + params.style + "\"");
  response.write(">&nbsp;</body></html>");
}
