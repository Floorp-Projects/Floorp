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

  response.write('<html>\n' +
    '<head>\n' +
      '<title>Browser VKB Overlapping content</title>  <meta charset="utf-8">');

  if (params.metadata)
    response.write("<meta name=\"viewport\" content=\"" + params.metadata + "\"/>");

  /* Write a spacer div into the document, above an input element*/
  response.write('</head>\n' +
    '<body style="margin: 0; padding: 0">\n' +
      '<div style="width: 100%; height: 100%"></div>\n' +
      '<input type="text" style="background-color: green">\n' +
    '</body>\n</html>');
}
