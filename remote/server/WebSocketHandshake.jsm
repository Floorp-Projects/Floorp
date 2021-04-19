/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["WebSocketHandshake"];

// This file is an XPCOM service-ified copy of ../devtools/server/socket/websocket-server.js.

const CC = Components.Constructor;

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  executeSoon: "chrome://remote/content/shared/Sync.jsm",
});

XPCOMUtils.defineLazyGetter(this, "CryptoHash", () => {
  return CC("@mozilla.org/security/hash;1", "nsICryptoHash", "initWithString");
});

XPCOMUtils.defineLazyGetter(this, "threadManager", () => {
  return Cc["@mozilla.org/thread-manager;1"].getService();
});

// TODO(ato): Merge this with httpd.js so that we can respond to both HTTP/1.1
// as well as WebSocket requests on the same server.

/**
 * Write a string of bytes to async output stream
 * and return promise that resolves once all data has been written.
 * Doesn't do any UTF-16/UTF-8 conversion.
 * The string is treated as an array of bytes.
 */
function writeString(output, data) {
  return new Promise((resolve, reject) => {
    const wait = () => {
      if (data.length === 0) {
        resolve();
        return;
      }

      output.asyncWait(
        stream => {
          try {
            const written = output.write(data, data.length);
            data = data.slice(written);
            wait();
          } catch (ex) {
            reject(ex);
          }
        },
        0,
        0,
        threadManager.currentThread
      );
    };

    wait();
  });
}

/** Write HTTP response (array of strings) to async output stream. */
function writeHttpResponse(output, response) {
  const s = response.join("\r\n") + "\r\n\r\n";
  return writeString(output, s);
}

/**
 * Process the WebSocket handshake headers and return the key to be sent in
 * Sec-WebSocket-Accept response header.
 */
function processRequest({ requestLine, headers }) {
  const method = requestLine.split(" ")[0];
  if (method !== "GET") {
    throw new Error("The handshake request must use GET method");
  }

  const upgrade = headers.get("upgrade");
  if (!upgrade || upgrade.toLowerCase() !== "websocket") {
    throw new Error(
      `The handshake request has incorrect Upgrade header: ${upgrade}`
    );
  }

  const connection = headers.get("connection");
  if (
    !connection ||
    !connection
      .split(",")
      .map(t => t.trim().toLowerCase())
      .includes("upgrade")
  ) {
    throw new Error("The handshake request has incorrect Connection header");
  }

  const version = headers.get("sec-websocket-version");
  if (!version || version !== "13") {
    throw new Error(
      "The handshake request must have Sec-WebSocket-Version: 13"
    );
  }

  // Compute the accept key
  const key = headers.get("sec-websocket-key");
  if (!key) {
    throw new Error(
      "The handshake request must have a Sec-WebSocket-Key header"
    );
  }

  return { acceptKey: computeKey(key) };
}

function computeKey(key) {
  const str = `${key}258EAFA5-E914-47DA-95CA-C5AB0DC85B11`;
  const data = Array.from(str, ch => ch.charCodeAt(0));
  const hash = new CryptoHash("sha1");
  hash.update(data, data.length);
  return hash.finish(true);
}

/**
 * Perform the server part of a WebSocket opening handshake
 * on an incoming connection.
 */
async function serverHandshake(request, output) {
  try {
    // Check and extract info from the request
    const { acceptKey } = processRequest(request);

    // Send response headers
    await writeHttpResponse(output, [
      "HTTP/1.1 101 Switching Protocols",
      "Upgrade: websocket",
      "Connection: Upgrade",
      `Sec-WebSocket-Accept: ${acceptKey}`,
    ]);
  } catch (error) {
    // Send error response in case of error
    await writeHttpResponse(output, ["HTTP/1.1 400 Bad Request"]);
    throw error;
  }
}

async function createWebSocket(transport, input, output) {
  const transportProvider = {
    setListener(upgradeListener) {
      // onTransportAvailable callback shouldn't be called synchronously
      executeSoon(() => {
        upgradeListener.onTransportAvailable(transport, input, output);
      });
    },
  };

  return new Promise((resolve, reject) => {
    const socket = WebSocket.createServerWebSocket(
      null,
      [],
      transportProvider,
      ""
    );
    socket.addEventListener("close", () => {
      input.close();
      output.close();
    });

    socket.onopen = () => resolve(socket);
    socket.onerror = err => reject(err);
  });
}

/** Upgrade an existing HTTP request from httpd.js to WebSocket. */
async function upgrade(request, response) {
  // handle response manually, allowing us to send arbitrary data
  response._powerSeized = true;

  const { transport, input, output } = response._connection;

  const headers = new Map();
  for (let [key, values] of Object.entries(request._headers._headers)) {
    headers.set(key, values.join("\n"));
  }
  const convertedRequest = {
    requestLine: `${request.method} ${request.path}`,
    headers,
  };
  await serverHandshake(convertedRequest, output);

  return createWebSocket(transport, input, output);
}

const WebSocketHandshake = { upgrade };
