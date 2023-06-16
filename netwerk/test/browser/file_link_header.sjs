"use strict";

function handleRequest(request, response) {
  // write to raw socket
  response.seizePower();
  let body = `<!DOCTYPE html>
      <html>
      <head>
      <link rel="preconnect" href="https://localhost">
      </head>
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
