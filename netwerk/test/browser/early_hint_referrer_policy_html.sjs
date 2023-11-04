"use strict";

function handleRequest(request, response) {
  let qs = new URLSearchParams(request.queryString);
  let asset = qs.get("as");
  var action = qs.get("action");
  let hinted = qs.get("hinted") !== "0";
  let httpCode = qs.get("code");
  let header_referrer_policy = qs.get("header_referrer_policy");
  let link_referrer_policy = qs.get("link_referrer_policy");

  // eslint-disable-next-line mozilla/use-services
  let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(
    Ci.nsIUUIDGenerator
  );
  let uuid = uuidGenerator.generateUUID().toString();
  let url = `early_hint_pixel.sjs?as=${asset}&uuid=${uuid}`;

  if (action === "get_request_referrer_results") {
    response.setHeader("Cache-Control", "no-cache", false);
    response.setHeader("Content-Type", "text/plain", false);
    response.write(getSharedState("requestReferrer"));
    return;
  } else if (action === "reset_referrer_results") {
    response.setHeader("Cache-Control", "no-cache", false);
    response.setHeader("Content-Type", "text/plain", false);
    response.write(setSharedState("requestReferrer", "not set"));
    return;
  }

  // write to raw socket
  response.seizePower();

  if (hinted) {
    response.write("HTTP/1.1 103 Early Hint\r\n");

    if (header_referrer_policy) {
      response.write(
        `Referrer-Policy: ${header_referrer_policy.replaceAll('"', "")}\r\n`
      );
    }

    response.write(
      `Link: <${url}>; rel=preload; as=${asset}; ${
        link_referrer_policy ? "referrerpolicy=" + link_referrer_policy : ""
      } \r\n`
    );
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

  response.finish();
}
