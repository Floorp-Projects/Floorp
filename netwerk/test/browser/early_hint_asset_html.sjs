"use strict";

function handleRequest(request, response) {
  let qs = new URLSearchParams(request.queryString);
  let asset = qs.get("as");
  let hinted = qs.get("hinted") === "1";
  let httpCode = qs.get("code");
  let uuid = qs.get("uuid");
  let cached = qs.get("cached") === "1";

  let url = `early_hint_asset.sjs?as=${asset}${uuid ? `&uuid=${uuid}` : ""}${
    cached ? "&cached=1" : ""
  }`;

  // write to raw socket
  response.seizePower();
  let link = "";
  if (hinted) {
    response.write("HTTP/1.1 103 Early Hint\r\n");
    if (asset === "fetch" || asset === "font") {
      // fetch and font has to specify the crossorigin attribute
      // https://developer.mozilla.org/en-US/docs/Web/HTML/Element/link#attr-as
      link = `Link: <${url}>; rel=preload; as=${asset}; crossorigin=anonymous\r\n`;
      response.write(link);
    } else if (asset === "module") {
      // module preloads are handled differently
      link = `Link: <${url}>; rel=modulepreload\r\n`;
      response.write(link);
    } else {
      link = `Link: <${url}>; rel=preload; as=${asset}\r\n`;
      response.write(link);
    }
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
  } else if (asset === "module") {
    // this code assumes that the .sjs for the module is in the same directory
    var file_name = url.split("/");
    file_name = file_name[file_name.length - 1];
    body = `<!DOCTYPE html>
      <html>
      <head>
      </head>
      <body>
      <h1>Test preload module<h1>
      <div id="square" style="width:100px;height:100px;">
      <script type="module">
        import { draw } from "./${file_name}";
        draw();
      </script>
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
        src: url("${url}");
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
  response.write(link);
  response.write("Content-Type: text/html;charset=utf-8\r\n");
  response.write("Cache-Control: no-cache\r\n");
  response.write(`Content-Length: ${body.length}\r\n`);
  response.write("\r\n");
  response.write(body);
  response.finish();
}
