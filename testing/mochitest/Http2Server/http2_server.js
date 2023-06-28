/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals require, __dirname, global, Buffer, process */

const fs = require("fs");
const options = {
  key: fs.readFileSync(__dirname + "/mochitest-cert.key.pem"),
  cert: fs.readFileSync(__dirname + "/mochitest-cert.pem"),
  settings: {
    enableConnectProtocol: true,
  },
};
const http2 = require("http2");
const http = require("http");
const url = require("url");
const path = require("path");

// This is the path of node-ws when running mochitest locally.
let node_ws_root = path.join(__dirname, "../../xpcshell/node-ws");
if (!fs.existsSync(node_ws_root)) {
  // This path is for running mochitest on try.
  node_ws_root = path.join(__dirname, "./node_ws");
}

const WebSocket = require(`${node_ws_root}/lib/websocket`);

let listeningPort = parseInt(process.argv[3].split("=")[1]);
let log = function () {};

function handle_h2_non_connect(stream, headers) {
  const session = stream.session;
  const uri = new URL(
    `${headers[":scheme"]}://${headers[":authority"]}${headers[":path"]}`
  );
  const url = uri.toString();

  log("REQUEST:", url);
  log("REQUEST HEADER:", JSON.stringify(headers));

  stream.on("close", () => {
    log("REQUEST STREAM CLOSED:", stream.rstCode);
  });
  stream.on("error", error => {
    log("RESPONSE STREAM ERROR", error, url, "on session:", session.__id);
  });

  let newHeaders = {};
  for (let key in headers) {
    if (!key.startsWith(":")) {
      newHeaders[key] = headers[key];
    }
  }

  const options = {
    protocol: "http:",
    hostname: "127.0.0.1",
    port: 8888,
    path: headers[":path"],
    method: headers[":method"],
    headers: newHeaders,
  };

  log("OPTION:", JSON.stringify(options));
  const request = http.request(options);

  stream.pipe(request);

  request.on("response", response => {
    const headers = Object.fromEntries(
      Object.entries(response.headers).filter(
        ([key]) =>
          !["connection", "transfer-encoding", "keep-alive"].includes(key)
      )
    );
    headers[":status"] = response.statusCode;
    log("RESPONSE BEGIN", url, headers, "on session:", session.__id);

    try {
      stream.respond(headers);

      response.on("data", data => {
        log("RESPONSE DATA", data.length, url);
        stream.write(data);
      });
      response.on("error", error => {
        log("RESPONSE ERROR", error, url, "on session:", session.__id);
        stream.close(http2.constants.NGHTTP2_REFUSED_STREAM);
      });
      response.on("end", () => {
        log("RESPONSE END", url, "on session:", session.__id);
        stream.end();
      });
    } catch (exception) {
      log("RESPONSE EXCEPTION", exception, url, "on session:", session.__id);
      stream.close();
    }
  });
  request.on("error", error => {
    console.error("REQUEST ERROR", error, url, "on session:", session.__id);
    try {
      stream.respond({
        ":status": 502,
        "content-type": "application/proxy-explanation+json",
      });
      stream.end(
        JSON.stringify({
          title: "request error",
          description: error.toString(),
        })
      );
    } catch (exception) {
      stream.close();
    }
  });
}

let server = http2.createSecureServer(options);

let session_count = 0;
let session_id = 0;

server.on("session", session => {
  session.__id = ++session_id;
  session.__tunnel_count = 0;

  ++session_count;
  if (session_count === 1) {
    log(`\n\n>>> FIRST SESSION OPENING\n`);
  }
  log(`*** NEW SESSION`, session.__id, "( sessions:", session_count, ")");

  session.on("error", error => {
    console.error("SESSION ERROR", session.__id, error);
  });
  session.on("close", () => {
    --session_count;
    log(`*** CLOSED SESSION`, session.__id, "( sessions:", session_count, ")");
    if (!session_count) {
      log(`\n\n<<< LAST SESSION CLOSED\n`);
    }
  });
});

server.on("stream", (stream, headers) => {
  if (headers[":method"] === "CONNECT") {
    stream.respond();

    const ws = new WebSocket(null);
    stream.setNoDelay = () => {};
    ws.setSocket(stream, Buffer.from(""), 100 * 1024 * 1024);

    ws.on("message", data => {
      ws.send(data);
    });
  } else {
    handle_h2_non_connect(stream, headers);
  }
});

server.listen(listeningPort);

console.log(`Http2 server listening on ports ${server.address().port}`);
