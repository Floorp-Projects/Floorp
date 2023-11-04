"use strict";

function handleRequest(request, response) {
  let qs = new URLSearchParams(request.queryString);
  let asset = qs.get("as");
  let hinted = qs.get("hinted") !== "0";
  let httpCode = qs.get("code");
  let csp = qs.get("csp");
  let csp_in_early_hint = qs.get("csp_in_early_hint");
  let host = qs.get("host");

  // eslint-disable-next-line mozilla/use-services
  let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(
    Ci.nsIUUIDGenerator
  );
  let uuid = uuidGenerator.generateUUID().toString();
  let url = `early_hint_pixel.sjs?as=${asset}&uuid=${uuid}`;
  if (host) {
    url = host + url;
  }

  // write to raw socket
  response.seizePower();

  if (hinted) {
    response.write("HTTP/1.1 103 Early Hint\r\n");
    if (csp_in_early_hint) {
      response.write(
        `Content-Security-Policy: ${csp_in_early_hint.replaceAll('"', "")}\r\n`
      );
    }
    response.write(`Link: <${url}>; rel=preload; as=${asset}\r\n`);
    response.write("\r\n");
  }

  let body = "";
  if (asset === "image") {
    body = `<!DOCTYPE html>
      <html>
      <body>
      <img id="test_image" src="${url}" width="100px">
      </body>
      </html>`;
  } else if (asset === "style") {
    body = `<!DOCTYPE html>
      <html>
      <head>
      <link rel="stylesheet" type="text/css" href="${url}">
      </head>
      <body>
      <h1>Test preload css<h1>
      <div id="square" style="width:100px;height:100px;">
      </body>
      </html>
    `;
  } else if (asset === "script") {
    body = `<!DOCTYPE html>
      <html>
      <head>
      <script src="${url}"></script>
      </head>
      <body>
      <h1>Test preload javascript<h1>
      <div id="square" style="width:100px;height:100px;">
      </body>
      </html>
    `;
  } else if (asset === "fetch") {
    body = `<!DOCTYPE html>
      <html>
      <body onload="onLoad()">
      <script>
      function onLoad() {
        fetch("${url}")
          .then(r => r.text())
          .then(r => document.getElementsByTagName("h2")[0].textContent = r);
      }
      </script>
      <h1>Test preload fetch</h1>
      <h2>Fetching...</h2>
      </body>
      </html>
    `;
  } else if (asset === "font") {
    body = `<!DOCTYPE html>
    <html>
    <head>
    <style>
    @font-face {
      font-family: "preloadFont";
      src: url("${url}") format("woff");
    }
    body {
      font-family: "preloadFont";
    }
    </style>
    </head>
    <body>
    <h1>Test preload font<h1>
    </body>
    </html>
  `;
  }

  if (!httpCode) {
    response.write(`HTTP/1.1 200 OK\r\n`);
  } else {
    response.write(`HTTP/1.1 ${httpCode} Error\r\n`);
  }
  response.write("Content-Type: text/html;charset=utf-8\r\n");
  response.write("Cache-Control: no-cache\r\n");
  response.write(`Content-Length: ${body.length}\r\n`);
  if (csp) {
    response.write(`Content-Security-Policy: ${csp.replaceAll('"', "")}\r\n`);
  }
  response.write("\r\n");
  response.write(body);

  response.finish();
}
