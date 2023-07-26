/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is an XPCOM service-ified copy of ../devtools/server/socket/websocket-server.js.

const CC = Components.Constructor;

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  executeSoon: "chrome://remote/content/shared/Sync.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  RemoteAgent: "chrome://remote/content/components/RemoteAgent.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

ChromeUtils.defineLazyGetter(lazy, "CryptoHash", () => {
  return CC("@mozilla.org/security/hash;1", "nsICryptoHash", "initWithString");
});

ChromeUtils.defineLazyGetter(lazy, "threadManager", () => {
  return Cc["@mozilla.org/thread-manager;1"].getService();
});

/**
 * Allowed origins are exposed through 2 separate getters because while most
 * of the values should be valid URIs, `null` is also a valid origin and cannot
 * be converted to a URI. Call sites interested in checking for null should use
 * `allowedOrigins`, those interested in URIs should use `allowedOriginURIs`.
 */
ChromeUtils.defineLazyGetter(lazy, "allowedOrigins", () =>
  lazy.RemoteAgent.allowOrigins !== null ? lazy.RemoteAgent.allowOrigins : []
);

ChromeUtils.defineLazyGetter(lazy, "allowedOriginURIs", () => {
  return lazy.allowedOrigins
    .map(origin => {
      try {
        const originURI = Services.io.newURI(origin);
        // Make sure to read host/port/scheme as those getters could throw for
        // invalid URIs.
        return {
          host: originURI.host,
          port: originURI.port,
          scheme: originURI.scheme,
        };
      } catch (e) {
        return null;
      }
    })
    .filter(uri => uri !== null);
});

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
        lazy.threadManager.currentThread
      );
    };

    wait();
  });
}

/**
 * Write HTTP response with headers (array of strings) and body
 * to async output stream.
 */
function writeHttpResponse(output, headers, body = "") {
  headers.push(`Content-Length: ${body.length}`);

  const s = headers.join("\r\n") + `\r\n\r\n${body}`;
  return writeString(output, s);
}

/**
 * Check if the provided URI's host is an IP address.
 *
 * @param {nsIURI} uri
 *     The URI to check.
 * @returns {boolean}
 */
function isIPAddress(uri) {
  try {
    // getBaseDomain throws an explicit error if the uri host is an IP address.
    Services.eTLD.getBaseDomain(uri);
  } catch (e) {
    return e.result == Cr.NS_ERROR_HOST_IS_IP_ADDRESS;
  }
  return false;
}

function isHostValid(hostHeader) {
  try {
    // Might throw both when calling newURI or when accessing the host/port.
    const hostUri = Services.io.newURI(`https://${hostHeader}`);
    const { host, port } = hostUri;
    const isHostnameValid =
      isIPAddress(hostUri) || lazy.RemoteAgent.allowHosts.includes(host);
    // For nsIURI a port value of -1 corresponds to the protocol's default port.
    const isPortValid = [-1, lazy.RemoteAgent.port].includes(port);
    return isHostnameValid && isPortValid;
  } catch (e) {
    return false;
  }
}

function isOriginValid(originHeader) {
  if (originHeader === undefined) {
    // Always accept no origin header.
    return true;
  }

  // Special case "null" origins, used for privacy sensitive or opaque origins.
  if (originHeader === "null") {
    return lazy.allowedOrigins.includes("null");
  }

  try {
    // Extract the host, port and scheme from the provided origin header.
    const { host, port, scheme } = Services.io.newURI(originHeader);
    // Check if any allowed origin matches the provided host, port and scheme.
    return lazy.allowedOriginURIs.some(
      uri => uri.host === host && uri.port === port && uri.scheme === scheme
    );
  } catch (e) {
    // Reject invalid origin headers
    return false;
  }
}

/**
 * Process the WebSocket handshake headers and return the key to be sent in
 * Sec-WebSocket-Accept response header.
 */
function processRequest({ requestLine, headers }) {
  if (!isOriginValid(headers.get("origin"))) {
    lazy.logger.debug(
      `Incorrect Origin header, allowed origins: [${lazy.allowedOrigins}]`
    );
    throw new Error(
      `The handshake request has incorrect Origin header ${headers.get(
        "origin"
      )}`
    );
  }

  if (!isHostValid(headers.get("host"))) {
    lazy.logger.debug(
      `Incorrect Host header, allowed hosts: [${lazy.RemoteAgent.allowHosts}]`
    );
    throw new Error(
      `The handshake request has incorrect Host header ${headers.get("host")}`
    );
  }

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
  const hash = new lazy.CryptoHash("sha1");
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
      "Server: httpd.js",
      "Upgrade: websocket",
      "Connection: Upgrade",
      `Sec-WebSocket-Accept: ${acceptKey}`,
    ]);
  } catch (error) {
    // Send error response in case of error
    await writeHttpResponse(
      output,
      [
        "HTTP/1.1 400 Bad Request",
        "Server: httpd.js",
        "Content-Type: text/plain",
      ],
      error.message
    );

    throw error;
  }
}

async function createWebSocket(transport, input, output) {
  const transportProvider = {
    setListener(upgradeListener) {
      // onTransportAvailable callback shouldn't be called synchronously
      lazy.executeSoon(() => {
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

  lazy.logger.info(
    `Perform WebSocket upgrade for incoming connection from ${transport.host}:${transport.port}`
  );

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

export const WebSocketHandshake = { upgrade };
