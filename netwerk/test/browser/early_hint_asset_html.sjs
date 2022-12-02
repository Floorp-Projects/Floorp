"use strict";

function handleRequest(request, response) {
  Cu.importGlobalProperties(["URLSearchParams"]);
  let qs = new URLSearchParams(request.queryString);
  let asset = qs.get("as");
  let hinted = qs.get("hinted") !== "0";
  let httpCode = qs.get("code");
  let redirect = qs.get("redirect") === "1";
  let crossOrigin = qs.get("crossOrigin") === "1";

  // eslint-disable-next-line mozilla/use-services
  let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(
    Ci.nsIUUIDGenerator
  );
  let uuid = uuidGenerator.generateUUID().toString();
  let url = `early_hint_asset.sjs?as=${asset}&uuid=${uuid}`;

  // write to raw socket
  response.seizePower();

  if (hinted) {
    response.write("HTTP/1.1 103 Early Hint\r\n");
    response.write(`Link: <${url}>; rel=preload; as=${asset}\r\n`);
    response.write("\r\n");
  }

  let body = "";
  if (asset === "image") {
    body = `<!DOCTYPE html>
      <html>
      <body>
      <img src="${url}" width="100px">
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

  if (redirect) {
    response.write(`HTTP/1.1 301 Moved Permanently\r\n`);
    let redirectUrl = crossOrigin
      ? `https://example.net/browser/netwerk/test/browser/early_hint_main_html.sjs`
      : `https://example.com/browser/netwerk/test/browser/early_hint_main_html.sjs`;

    response.write(`Location: ${redirectUrl}\r\n`);
    response.write("testing early hint redirect");
  } else {
    if (!httpCode) {
      response.write(`HTTP/1.1 200 OK\r\n`);
    } else {
      response.write(`HTTP/1.1 ${httpCode} Error\r\n`);
    }
    response.write("Content-Type: text/html;charset=utf-8\r\n");
    response.write("Cache-Control: no-cache\r\n");
    response.write(`Content-Length: ${body.length}\r\n`);
    response.write("\r\n");
    response.write(body);
  }
  response.finish();
}
