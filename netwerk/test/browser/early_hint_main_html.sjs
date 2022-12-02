"use strict";

function handleRequest(request, response) {
  Cu.importGlobalProperties(["URLSearchParams"]);

  // write to raw socket
  response.seizePower();

  let qs = new URLSearchParams(request.queryString);
  let imgs = [];
  let new_hint = true;
  let new_header = true;
  for (const [imgUrl, uuid] of qs.entries()) {
    if (new_hint) {
      // we need to write a new header
      new_hint = false;
      response.write("HTTP/1.1 103 Early Hint\r\n");
    }
    if (!imgUrl.length) {
      // next hint in new early hint response when empty string is passed
      new_header = true;
      if (uuid === "new_response") {
        new_hint = true;
        response.write("\r\n");
      }
      response.write("\r\n");
    } else {
      // either append link in new header or in same header
      if (new_header) {
        new_header = false;
        response.write("Link: ");
      } else {
        response.write(", ");
      }
      // add query string to make request unique this has the drawback that
      // the preloaded image can't accept query strings on it's own / or has
      // to strip the appended "?uuid" from the query string before parsing
      imgs.push(`<img src="${imgUrl}?${uuid}" width="100px">`);
      response.write(`<${imgUrl}?${uuid}>; rel=preload; as=image`);
    }
  }
  if (!new_hint) {
    // add separator to main document
    response.write("\r\n\r\n");
  }

  let body = `<!DOCTYPE html>
<html>
<body>
${imgs.join("\n")}
</body>
</html>`;

  // main document response
  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Content-Type: text/html;charset=utf-8\r\n");
  response.write("Cache-Control: no-cache\r\n");
  response.write(`Content-Length: ${body.length}\r\n`);
  response.write("\r\n");
  response.write(body);
  response.finish();
}
