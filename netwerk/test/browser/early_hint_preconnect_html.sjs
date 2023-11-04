"use strict";

function handleRequest(request, response) {
  let qs = new URLSearchParams(request.queryString);
  let href = qs.get("href");
  let crossOrigin = qs.get("crossOrigin");

  // write to raw socket
  response.seizePower();

  response.write("HTTP/1.1 103 Early Hint\r\n");
  response.write(
    `Link: <${href}>; rel=preconnect; crossOrigin=${crossOrigin}\r\n`
  );
  response.write("\r\n");

  let body = `<!DOCTYPE html>
      <html>
      <body>
      <h1>Test rel=preconnect<h1>
      </body>
      </html>`;

  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Content-Type: text/html;charset=utf-8\r\n");
  response.write("Cache-Control: no-cache\r\n");
  response.write(`Content-Length: ${body.length}\r\n`);
  response.write("\r\n");
  response.write(body);

  response.finish();
}
