/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This module is the stateful server side of test_http2.js and is meant
// to have node be restarted in between each invocation

/* eslint-env node */

var node_http2_root = "../node-http2";
if (process.env.NODE_HTTP2_ROOT) {
  node_http2_root = process.env.NODE_HTTP2_ROOT;
}
var http2 = require(node_http2_root);
var fs = require("fs");
var url = require("url");
var crypto = require("crypto");
const dnsPacket = require(`${node_http2_root}/../dns-packet`);
const ip = require(`${node_http2_root}/../node-ip`);
const { fork } = require("child_process");
const path = require("path");
const zlib = require("zlib");

// Hook into the decompression code to log the decompressed name-value pairs
var compression_module = node_http2_root + "/lib/protocol/compressor";
var http2_compression = require(compression_module);
var HeaderSetDecompressor = http2_compression.HeaderSetDecompressor;
var originalRead = HeaderSetDecompressor.prototype.read;
var lastDecompressor;
var decompressedPairs;
HeaderSetDecompressor.prototype.read = function() {
  if (this != lastDecompressor) {
    lastDecompressor = this;
    decompressedPairs = [];
  }
  var pair = originalRead.apply(this, arguments);
  if (pair) {
    decompressedPairs.push(pair);
  }
  return pair;
};

var connection_module = node_http2_root + "/lib/protocol/connection";
var http2_connection = require(connection_module);
var Connection = http2_connection.Connection;
var originalClose = Connection.prototype.close;
Connection.prototype.close = function(error, lastId) {
  if (lastId !== undefined) {
    this._lastIncomingStream = lastId;
  }

  originalClose.apply(this, arguments);
};

var framer_module = node_http2_root + "/lib/protocol/framer";
var http2_framer = require(framer_module);
var Serializer = http2_framer.Serializer;
var originalTransform = Serializer.prototype._transform;
var newTransform = function(frame, encoding, done) {
  if (frame.type == "DATA") {
    // Insert our empty DATA frame
    const emptyFrame = {};
    emptyFrame.type = "DATA";
    emptyFrame.data = Buffer.alloc(0);
    emptyFrame.flags = [];
    emptyFrame.stream = frame.stream;
    var buffers = [];
    Serializer.DATA(emptyFrame, buffers);
    Serializer.commonHeader(emptyFrame, buffers);
    for (var i = 0; i < buffers.length; i++) {
      this.push(buffers[i]);
    }

    // Reset to the original version for later uses
    Serializer.prototype._transform = originalTransform;
  }
  originalTransform.apply(this, arguments);
};

function getHttpContent(pathName) {
  var content =
    "<!doctype html>" +
    "<html>" +
    "<head><title>HOORAY!</title></head>" +
    // 'You Win!' used in tests to check we reached this server
    "<body>You Win! (by requesting" +
    pathName +
    ")</body>" +
    "</html>";
  return content;
}

function generateContent(size) {
  var content = "";
  for (var i = 0; i < size; i++) {
    content += "0";
  }
  return content;
}

/* This takes care of responding to the multiplexed request for us */
var m = {
  mp1res: null,
  mp2res: null,
  buf: null,
  mp1start: 0,
  mp2start: 0,

  checkReady() {
    if (this.mp1res != null && this.mp2res != null) {
      this.buf = generateContent(30 * 1024);
      this.mp1start = 0;
      this.mp2start = 0;
      this.send(this.mp1res, 0);
      setTimeout(this.send.bind(this, this.mp2res, 0), 5);
    }
  },

  send(res, start) {
    var end = Math.min(start + 1024, this.buf.length);
    var content = this.buf.substring(start, end);
    res.write(content);
    if (end < this.buf.length) {
      setTimeout(this.send.bind(this, res, end), 10);
    } else {
      res.end();
    }
  },
};

var runlater = function() {};
runlater.prototype = {
  req: null,
  resp: null,

  onTimeout: function onTimeout() {
    this.resp.writeHead(200);
    this.resp.end("It's all good 750ms.");
  },
};

var moreData = function() {};
moreData.prototype = {
  req: null,
  resp: null,
  iter: 3,

  onTimeout: function onTimeout() {
    // 1mb of data
    const content = generateContent(1024 * 1024);
    this.resp.write(content); // 1mb chunk
    this.iter--;
    if (!this.iter) {
      this.resp.end();
    } else {
      setTimeout(executeRunLater, 1, this);
    }
  },
};

function executeRunLater(arg) {
  arg.onTimeout();
}

var Compressor = http2_compression.Compressor;
var HeaderSetCompressor = http2_compression.HeaderSetCompressor;
var originalCompressHeaders = Compressor.prototype.compress;

function insertSoftIllegalHpack(headers) {
  var originalCompressed = originalCompressHeaders.apply(this, headers);
  var illegalLiteral = Buffer.from([
    0x00, // Literal, no index
    0x08, // Name: not huffman encoded, 8 bytes long
    0x3a,
    0x69,
    0x6c,
    0x6c,
    0x65,
    0x67,
    0x61,
    0x6c, // :illegal
    0x10, // Value: not huffman encoded, 16 bytes long
    // REALLY NOT LEGAL
    0x52,
    0x45,
    0x41,
    0x4c,
    0x4c,
    0x59,
    0x20,
    0x4e,
    0x4f,
    0x54,
    0x20,
    0x4c,
    0x45,
    0x47,
    0x41,
    0x4c,
  ]);
  var newBufferLength = originalCompressed.length + illegalLiteral.length;
  var concatenated = Buffer.alloc(newBufferLength);
  originalCompressed.copy(concatenated, 0);
  illegalLiteral.copy(concatenated, originalCompressed.length);
  return concatenated;
}

function insertHardIllegalHpack(headers) {
  var originalCompressed = originalCompressHeaders.apply(this, headers);
  // Now we have to add an invalid header
  var illegalIndexed = HeaderSetCompressor.integer(5000, 7);
  // The above returns an array of buffers, but there's only one buffer, so
  // get rid of the array.
  illegalIndexed = illegalIndexed[0];
  // Set the first bit to 1 to signal this is an indexed representation
  illegalIndexed[0] |= 0x80;
  var newBufferLength = originalCompressed.length + illegalIndexed.length;
  var concatenated = Buffer.alloc(newBufferLength);
  originalCompressed.copy(concatenated, 0);
  illegalIndexed.copy(concatenated, originalCompressed.length);
  return concatenated;
}

var h11required_conn = null;
var h11required_header = "yes";
var didRst = false;
var rstConnection = null;
var illegalheader_conn = null;

var cname_confirm = 0;

// eslint-disable-next-line complexity
function handleRequest(req, res) {
  // We do this first to ensure nothing goes wonky in our tests that don't want
  // the headers to have something illegal in them
  Compressor.prototype.compress = originalCompressHeaders;

  var u = url.parse(req.url, true);
  var content = getHttpContent(u.pathname);
  var push, push1, push1a, push2, push3;

  // PushService tests.
  var pushPushServer1, pushPushServer2, pushPushServer3, pushPushServer4;

  if (req.httpVersionMajor === 2) {
    res.setHeader("X-Connection-Http2", "yes");
    res.setHeader("X-Http2-StreamId", "" + req.stream.id);
  } else {
    res.setHeader("X-Connection-Http2", "no");
  }

  if (u.pathname === "/exit") {
    res.setHeader("Content-Type", "text/plain");
    res.setHeader("Connection", "close");
    res.writeHead(200);
    res.end("ok");
    process.exit();
  }

  if (u.pathname === "/750ms") {
    let rl = new runlater();
    rl.req = req;
    rl.resp = res;
    setTimeout(executeRunLater, 750, rl);
    return;
  } else if (u.pathname === "/multiplex1" && req.httpVersionMajor === 2) {
    res.setHeader("Content-Type", "text/plain");
    res.writeHead(200);
    m.mp1res = res;
    m.checkReady();
    return;
  } else if (u.pathname === "/multiplex2" && req.httpVersionMajor === 2) {
    res.setHeader("Content-Type", "text/plain");
    res.writeHead(200);
    m.mp2res = res;
    m.checkReady();
    return;
  } else if (u.pathname === "/header") {
    var val = req.headers["x-test-header"];
    if (val) {
      res.setHeader("X-Received-Test-Header", val);
    }
  } else if (u.pathname === "/doubleheader") {
    res.setHeader("Content-Type", "text/html");
    res.writeHead(200);
    res.write(content);
    res.writeHead(200);
    res.end();
    return;
  } else if (u.pathname === "/cookie_crumbling") {
    res.setHeader("X-Received-Header-Pairs", JSON.stringify(decompressedPairs));
  } else if (u.pathname === "/push") {
    push = res.push("/push.js");
    push.writeHead(200, {
      "content-type": "application/javascript",
      pushed: "yes",
      "content-length": 11,
      "X-Connection-Http2": "yes",
    });
    push.end("// comments");
    content = '<head> <script src="push.js"/></head>body text';
  } else if (u.pathname === "/push.js") {
    content = "// comments";
    res.setHeader("pushed", "no");
  } else if (u.pathname === "/push2") {
    push = res.push("/push2.js");
    push.writeHead(200, {
      "content-type": "application/javascript",
      pushed: "yes",
      // no content-length
      "X-Connection-Http2": "yes",
    });
    push.end("// comments");
    content = '<head> <script src="push2.js"/></head>body text';
  } else if (u.pathname === "/push5") {
    push = res.push("/push5.js");
    push.writeHead(200, {
      "content-type": "application/javascript",
      pushed: "yes",
      // no content-length
      "X-Connection-Http2": "yes",
    });
    content = generateContent(1024 * 150);
    push.write(content);
    push.end();
    content = '<head> <script src="push5.js"/></head>body text';
  } else if (u.pathname === "/pushapi1") {
    push1 = res.push({
      hostname: "localhost:" + serverPort,
      port: serverPort,
      path: "/pushapi1/1",
      method: "GET",
      headers: { "x-pushed-request": "true", "x-foo": "bar" },
    });
    push1.writeHead(200, {
      pushed: "yes",
      "content-length": 1,
      subresource: "1",
      "X-Connection-Http2": "yes",
    });
    push1.end("1");

    push1a = res.push({
      hostname: "localhost:" + serverPort,
      port: serverPort,
      path: "/pushapi1/1",
      method: "GET",
      headers: { "x-foo": "bar", "x-pushed-request": "true" },
    });
    push1a.writeHead(200, {
      pushed: "yes",
      "content-length": 1,
      subresource: "1a",
      "X-Connection-Http2": "yes",
    });
    push1a.end("1");

    push2 = res.push({
      hostname: "localhost:" + serverPort,
      port: serverPort,
      path: "/pushapi1/2",
      method: "GET",
      headers: { "x-pushed-request": "true" },
    });
    push2.writeHead(200, {
      pushed: "yes",
      subresource: "2",
      "content-length": 1,
      "X-Connection-Http2": "yes",
    });
    push2.end("2");

    push3 = res.push({
      hostname: "localhost:" + serverPort,
      port: serverPort,
      path: "/pushapi1/3",
      method: "GET",
      headers: { "x-pushed-request": "true", "Accept-Encoding": "br" },
    });
    push3.writeHead(200, {
      pushed: "yes",
      "content-length": 6,
      subresource: "3",
      "content-encoding": "br",
      "X-Connection-Http2": "yes",
    });
    push3.end(Buffer.from([0x8b, 0x00, 0x80, 0x33, 0x0a, 0x03])); // '3\n'

    content = "0";
  } else if (u.pathname === "/big") {
    content = generateContent(128 * 1024);
    var hash = crypto.createHash("md5");
    hash.update(content);
    let md5 = hash.digest("hex");
    res.setHeader("X-Expected-MD5", md5);
  } else if (u.pathname === "/huge") {
    content = generateContent(1024);
    res.setHeader("Content-Type", "text/plain");
    res.writeHead(200);
    // 1mb of data
    for (let i = 0; i < 1024 * 1; i++) {
      res.write(content); // 1kb chunk
    }
    res.end();
    return;
  } else if (u.pathname === "/post" || u.pathname === "/patch") {
    if (req.method != "POST" && req.method != "PATCH") {
      res.writeHead(405);
      res.end("Unexpected method: " + req.method);
      return;
    }

    var post_hash = crypto.createHash("md5");
    req.on("data", function receivePostData(chunk) {
      post_hash.update(chunk.toString());
    });
    req.on("end", function finishPost() {
      let md5 = post_hash.digest("hex");
      res.setHeader("X-Calculated-MD5", md5);
      res.writeHead(200);
      res.end(content);
    });

    return;
  } else if (u.pathname === "/750msPost") {
    if (req.method != "POST") {
      res.writeHead(405);
      res.end("Unexpected method: " + req.method);
      return;
    }

    var accum = 0;
    req.on("data", function receivePostData(chunk) {
      accum += chunk.length;
    });
    req.on("end", function finishPost() {
      res.setHeader("X-Recvd", accum);
      let rl = new runlater();
      rl.req = req;
      rl.resp = res;
      setTimeout(executeRunLater, 750, rl);
    });

    return;
  } else if (u.pathname === "/h11required_stream") {
    if (req.httpVersionMajor === 2) {
      h11required_conn = req.stream.connection;
      res.stream.reset("HTTP_1_1_REQUIRED");
      return;
    }
  } else if (u.pathname === "/bigdownload") {
    res.setHeader("Content-Type", "text/html");
    res.writeHead(200);

    let rl = new moreData();
    rl.req = req;
    rl.resp = res;
    setTimeout(executeRunLater, 1, rl);
    return;
  } else if (u.pathname === "/h11required_session") {
    if (req.httpVersionMajor === 2) {
      if (h11required_conn !== req.stream.connection) {
        h11required_header = "no";
      }
      res.stream.connection.close("HTTP_1_1_REQUIRED", res.stream.id - 2);
      return;
    }
    res.setHeader("X-H11Required-Stream-Ok", h11required_header);
  } else if (u.pathname === "/rstonce") {
    if (!didRst && req.httpVersionMajor === 2) {
      didRst = true;
      rstConnection = req.stream.connection;
      req.stream.reset("REFUSED_STREAM");
      return;
    }

    if (rstConnection === null || rstConnection !== req.stream.connection) {
      res.setHeader("Connection", "close");
      res.writeHead(400);
      res.end("WRONG CONNECTION, HOMIE!");
      return;
    }

    if (req.httpVersionMajor != 2) {
      res.setHeader("Connection", "close");
    }
    res.writeHead(200);
    res.end("It's all good.");
    return;
  } else if (u.pathname === "/continuedheaders") {
    var pushRequestHeaders = { "x-pushed-request": "true" };
    var pushResponseHeaders = {
      "content-type": "text/plain",
      "content-length": "2",
      "X-Connection-Http2": "yes",
    };
    var pushHdrTxt =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    var pullHdrTxt = pushHdrTxt
      .split("")
      .reverse()
      .join("");
    for (let i = 0; i < 265; i++) {
      pushRequestHeaders["X-Push-Test-Header-" + i] = pushHdrTxt;
      res.setHeader("X-Pull-Test-Header-" + i, pullHdrTxt);
    }
    push = res.push({
      hostname: "localhost:" + serverPort,
      port: serverPort,
      path: "/continuedheaders/push",
      method: "GET",
      headers: pushRequestHeaders,
    });
    push.writeHead(200, pushResponseHeaders);
    push.end("ok");
  } else if (u.pathname === "/altsvc1") {
    if (
      req.httpVersionMajor != 2 ||
      req.scheme != "http" ||
      req.headers["alt-used"] != "foo.example.com:" + serverPort
    ) {
      res.writeHead(400);
      res.end("WHAT?");
      return;
    }
    // test the alt svc frame for use with altsvc2
    res.altsvc(
      "foo.example.com",
      serverPort,
      "h2",
      3600,
      req.headers["x-redirect-origin"]
    );
  } else if (u.pathname === "/altsvc2") {
    if (
      req.httpVersionMajor != 2 ||
      req.scheme != "http" ||
      req.headers["alt-used"] != "foo.example.com:" + serverPort
    ) {
      res.writeHead(400);
      res.end("WHAT?");
      return;
    }
  }

  // for use with test_altsvc.js
  else if (u.pathname === "/altsvc-test") {
    res.setHeader("Cache-Control", "no-cache");
    res.setHeader("Alt-Svc", "h2=" + req.headers["x-altsvc"]);
  }
  // for use with test_http3.js
  else if (u.pathname === "/http3-test") {
    res.setHeader("Cache-Control", "no-cache");
    res.setHeader("Alt-Svc", "h3-27=" + req.headers["x-altsvc"]);
  }
  // for use with test_trr.js
  else if (u.pathname === "/dns-cname") {
    // asking for cname.example.com
    let rContent;
    if (0 == cname_confirm) {
      // ... this sends a CNAME back to pointing-elsewhere.example.com
      rContent = Buffer.from(
        "00000100000100010000000005636E616D65076578616D706C6503636F6D0000050001C00C0005000100000037002012706F696E74696E672D656C73657768657265076578616D706C6503636F6D00",
        "hex"
      );
      cname_confirm++;
    } else {
      // ... this sends an A 99.88.77.66 entry back for pointing-elsewhere.example.com
      rContent = Buffer.from(
        "00000100000100010000000012706F696E74696E672D656C73657768657265076578616D706C6503636F6D0000010001C00C0001000100000037000463584D42",
        "hex"
      );
    }
    res.setHeader("Content-Type", "application/dns-message");
    res.setHeader("Content-Length", rContent.length);
    res.writeHead(200);
    res.write(rContent);
    res.end("");
    return;
  } else if (u.pathname == "/doh") {
    cname_confirm = 0; // back to first reply for dns-cname

    let responseIP = u.query.responseIP;
    if (!responseIP) {
      responseIP = "5.5.5.5";
    }

    let redirect = u.query.redirect;
    if (redirect) {
      responseIP = redirect;
      if (u.query.dns) {
        res.setHeader(
          "Location",
          "https://localhost:" +
            serverPort +
            "/doh?responseIP=" +
            responseIP +
            "&dns=" +
            u.query.dns
        );
      } else {
        res.setHeader(
          "Location",
          "https://localhost:" + serverPort + "/doh?responseIP=" + responseIP
        );
      }
      res.writeHead(307);
      res.end("");
      return;
    }

    if (u.query.auth) {
      // There's a Set-Cookie: header in the response for "/dns" , which this
      // request subsequently would include if the http channel wasn't
      // anonymous. Thus, if there's a cookie in this request, we know Firefox
      // mishaved. If there's not, we're fine.
      if (req.headers.cookie) {
        res.writeHead(403);
        res.end("cookie for me, not for you");
        return;
      }
      if (req.headers.authorization != "user:password") {
        res.writeHead(401);
        res.end("bad boy!");
        return;
      }
    }

    if (u.query.push) {
      // push.example.org has AAAA entry 2018::2018
      let pcontent = dnsPacket.encode({
        id: 0,
        type: "response",
        flags: dnsPacket.RECURSION_DESIRED,
        questions: [{ name: "push.example.org", type: "AAAA", class: "IN" }],
        answers: [
          {
            name: "push.example.org",
            type: "AAAA",
            ttl: 55,
            class: "IN",
            flush: false,
            data: "2018::2018",
          },
        ],
      });
      push = res.push({
        hostname: "foo.example.com:" + serverPort,
        port: serverPort,
        path:
          "/dns-pushed-response?dns=AAAAAAABAAAAAAAABHB1c2gHZXhhbXBsZQNvcmcAABwAAQ",
        method: "GET",
        headers: {
          accept: "application/dns-message",
        },
      });
      push.writeHead(200, {
        "content-type": "application/dns-message",
        pushed: "yes",
        "content-length": pcontent.length,
        "X-Connection-Http2": "yes",
      });
      push.end(pcontent);
    }

    let payload = Buffer.from("");

    function emitResponse(response, requestPayload) {
      let packet = dnsPacket.decode(requestPayload);

      // This shuts down the connection so we can test if the client reconnects
      if (
        packet.questions.length > 0 &&
        packet.questions[0].name == "closeme.com"
      ) {
        response.stream.connection.close("INTERNAL_ERROR", response.stream.id);
        return;
      }

      function responseType() {
        if (
          packet.questions.length > 0 &&
          packet.questions[0].name == "confirm.example.com" &&
          packet.questions[0].type == "NS"
        ) {
          return "NS";
        }

        return ip.isV4Format(responseIP) ? "A" : "AAAA";
      }

      function responseData() {
        if (
          packet.questions.length > 0 &&
          packet.questions[0].name == "confirm.example.com" &&
          packet.questions[0].type == "NS"
        ) {
          return "ns.example.com";
        }

        return responseIP;
      }

      let answers = [];
      if (responseIP != "none" && responseType() == packet.questions[0].type) {
        answers.push({
          name: u.query.hostname ? u.query.hostname : packet.questions[0].name,
          ttl: 55,
          type: responseType(),
          flush: false,
          data: responseData(),
        });
      }

      // for use with test_esni_dns_fetch.js
      if (packet.questions[0].type == "TXT") {
        answers.push({
          name: packet.questions[0].name,
          type: packet.questions[0].type,
          ttl: 55,
          class: "IN",
          flush: false,
          data: Buffer.from(
            "62586B67646D39705932556761584D6762586B676347467A63336476636D513D",
            "hex"
          ),
        });
      } else if (packet.questions[0].type == "HTTPSSVC") {
        answers.push({
          name: packet.questions[0].name,
          type: packet.questions[0].type,
          ttl: 55,
          class: "IN",
          flush: false,
          data: {
            priority: 1,
            name: "some.domain.stuff.",
            values: [{ key: "esniconfig", value: "testytestystringstring" }],
          },
        });
      }

      if (u.query.cnameloop) {
        answers.push({
          name: "cname.example.com",
          type: "CNAME",
          ttl: 55,
          class: "IN",
          flush: false,
          data: "pointing-elsewhere.example.com",
        });
      }

      if (req.headers["accept-language"] || req.headers["user-agent"]) {
        // If we get this header, don't send back any response. This should
        // cause the tests to fail. This is easier then actually sending back
        // the header value into test_trr.js
        answers = [];
      }

      let buf = dnsPacket.encode({
        type: "response",
        id: packet.id,
        flags: dnsPacket.RECURSION_DESIRED,
        questions: packet.questions,
        answers,
      });

      function writeResponse(resp, buffer) {
        resp.setHeader("Set-Cookie", "trackyou=yes; path=/; max-age=100000;");
        resp.setHeader("Content-Type", "application/dns-message");
        if (req.headers["accept-encoding"].includes("gzip")) {
          zlib.gzip(buffer, function(err, result) {
            resp.setHeader("Content-Encoding", "gzip");
            resp.setHeader("Content-Length", result.length);
            resp.writeHead(200);
            res.end(result);
          });
        } else {
          resp.setHeader("Content-Length", buffer.length);
          resp.writeHead(200);
          resp.write(buffer);
          resp.end("");
        }
      }

      let delay = undefined;
      if (packet.questions[0].type == "A") {
        delay = u.query.delayIPv4;
      } else if (packet.questions[0].type == "AAAA") {
        delay = u.query.delayIPv6;
      }

      if (delay) {
        setTimeout(
          arg => {
            writeResponse(arg[0], arg[1]);
          },
          parseInt(delay),
          [response, buf]
        );
        return;
      }

      writeResponse(response, buf);
    }

    if (u.query.dns) {
      payload = Buffer.from(u.query.dns, "base64");
      emitResponse(res, payload);
      return;
    }

    req.on("data", function receiveData(chunk) {
      payload = Buffer.concat([payload, chunk]);
    });
    req.on("end", function finishedData() {
      // parload is empty when we send redirect response.
      if (payload.length) {
        emitResponse(res, payload);
      }
    });
    return;
  } else if (u.pathname === "/httpssvc") {
    let payload = Buffer.from("");
    req.on("data", function receiveData(chunk) {
      payload = Buffer.concat([payload, chunk]);
    });
    req.on("end", function finishedData() {
      let packet = dnsPacket.decode(payload);
      let answers = [];
      answers.push({
        name: packet.questions[0].name,
        type: packet.questions[0].type,
        ttl: 55,
        class: "IN",
        flush: false,
        data: {
          priority: 1,
          name: "h3pool",
          values: [
            { key: "alpn", value: "h2,h3" },
            { key: "no-default-alpn" },
            { key: "port", value: 8888 },
            { key: "ipv4hint", value: "1.2.3.4" },
            { key: "esniconfig", value: "123..." },
            { key: "ipv6hint", value: "::1" },
            { key: 30, value: "somelargestring" },
          ],
        },
      });
      answers.push({
        name: packet.questions[0].name,
        type: packet.questions[0].type,
        ttl: 55,
        class: "IN",
        flush: false,
        data: {
          priority: 2,
          name: ".",
          values: [
            { key: "alpn", value: "h2" },
            { key: "ipv4hint", value: ["1.2.3.4", "5.6.7.8"] },
            { key: "esniconfig", value: "abc..." },
            { key: "ipv6hint", value: ["::1", "fe80::794f:6d2c:3d5e:7836"] },
          ],
        },
      });
      answers.push({
        name: packet.questions[0].name,
        type: packet.questions[0].type,
        ttl: 55,
        class: "IN",
        flush: false,
        data: {
          priority: 3,
          name: "hello",
          values: [],
        },
      });
      let buf = dnsPacket.encode({
        type: "response",
        id: packet.id,
        flags: dnsPacket.RECURSION_DESIRED,
        questions: packet.questions,
        answers,
      });

      res.setHeader("Content-Type", "application/dns-message");
      res.setHeader("Content-Length", buf.length);
      res.writeHead(200);
      res.write(buf);
      res.end("");
    });
    return;
  } else if (u.pathname === "/dns-cname-a") {
    // test23 asks for cname-a.example.com
    // this responds with a CNAME to here.example.com *and* an A record
    // for here.example.com
    let rContent;

    rContent = Buffer.from(
      "0000" +
      "0100" +
      "0001" + // QDCOUNT
      "0002" + // ANCOUNT
      "00000000" + // NSCOUNT + ARCOUNT
      "07636E616D652d61" + // cname-a
      "076578616D706C6503636F6D00" + // .example.com
      "00010001" + // question type (A) + question class (IN)
      // answer record 1
      "C00C" + // name pointer to cname-a.example.com
      "0005" + // type (CNAME)
      "0001" + // class
      "00000037" + // TTL
      "0012" + // RDLENGTH
      "0468657265" + // here
      "076578616D706C6503636F6D00" + // .example.com
      // answer record 2, the A entry for the CNAME above
      "0468657265" + // here
      "076578616D706C6503636F6D00" + // .example.com
      "0001" + // type (A)
      "0001" + // class
      "00000037" + // TTL
      "0004" + // RDLENGTH
        "09080706", // IPv4 address
      "hex"
    );
    res.setHeader("Content-Type", "application/dns-message");
    res.setHeader("Content-Length", rContent.length);
    res.writeHead(200);
    res.write(rContent);
    res.end("");
    return;
  } else if (u.pathname === "/dns-750ms") {
    // it's just meant to be this slow - the test doesn't care about the actual response
    return;
  }
  // for use with test_esni_dns_fetch.js
  else if (u.pathname === "/esni-dns-push") {
    // _esni_push.example.com has A entry 127.0.0.1
    let rContent = Buffer.from(
      "0000010000010001000000000A5F65736E695F70757368076578616D706C6503636F6D0000010001C00C000100010000003700047F000001",
      "hex"
    );

    // _esni_push.example.com has TXT entry 2062586B67646D39705932556761584D6762586B676347467A63336476636D513D
    var pcontent = Buffer.from(
      "0000818000010001000000000A5F65736E695F70757368076578616D706C6503636F6D0000100001C00C001000010000003700212062586B67646D39705932556761584D6762586B676347467A63336476636D513D",
      "hex"
    );

    push = res.push({
      hostname: "foo.example.com:" + serverPort,
      port: serverPort,
      path:
        "/dns-pushed-response?dns=AAABAAABAAAAAAABCl9lc25pX3B1c2gHZXhhbXBsZQNjb20AABAAAQAAKRAAAAAAAAAIAAgABAABAAA",
      method: "GET",
      headers: {
        accept: "application/dns-message",
      },
    });
    push.writeHead(200, {
      "content-type": "application/dns-message",
      pushed: "yes",
      "content-length": pcontent.length,
      "X-Connection-Http2": "yes",
    });
    push.end(pcontent);
    res.setHeader("Content-Type", "application/dns-message");
    res.setHeader("Content-Length", rContent.length);
    res.writeHead(200);
    res.write(rContent);
    res.end("");
    return;
  } else if (u.pathname === "/.well-known/http-opportunistic") {
    res.setHeader("Cache-Control", "no-cache");
    res.setHeader("Content-Type", "application/json");
    res.writeHead(200, "OK");
    res.end('["http://' + req.headers.host + '"]');
    return;
  } else if (u.pathname === "/stale-while-revalidate-loop-test") {
    res.setHeader(
      "Cache-Control",
      "s-maxage=86400, stale-while-revalidate=86400, immutable"
    );
    res.setHeader("Content-Type", "text/plain; charset=utf-8");
    res.setHeader("X-Content-Type-Options", "nosniff");
    res.setHeader("Content-Length", "1");
    res.writeHead(200, "OK");
    res.end("1");
    return;
  }

  // for PushService tests.
  else if (u.pathname === "/pushSubscriptionSuccess/subscribe") {
    res.setHeader(
      "Location",
      "https://localhost:" + serverPort + "/pushSubscriptionSuccesss"
    );
    res.setHeader(
      "Link",
      '</pushEndpointSuccess>; rel="urn:ietf:params:push", ' +
        '</receiptPushEndpointSuccess>; rel="urn:ietf:params:push:receipt"'
    );
    res.writeHead(201, "OK");
    res.end("");
    return;
  } else if (u.pathname === "/pushSubscriptionSuccesss") {
    // do nothing.
    return;
  } else if (u.pathname === "/pushSubscriptionMissingLocation/subscribe") {
    res.setHeader(
      "Link",
      '</pushEndpointMissingLocation>; rel="urn:ietf:params:push", ' +
        '</receiptPushEndpointMissingLocation>; rel="urn:ietf:params:push:receipt"'
    );
    res.writeHead(201, "OK");
    res.end("");
    return;
  } else if (u.pathname === "/pushSubscriptionMissingLink/subscribe") {
    res.setHeader(
      "Location",
      "https://localhost:" + serverPort + "/subscriptionMissingLink"
    );
    res.writeHead(201, "OK");
    res.end("");
    return;
  } else if (u.pathname === "/pushSubscriptionLocationBogus/subscribe") {
    res.setHeader("Location", "1234");
    res.setHeader(
      "Link",
      '</pushEndpointLocationBogus; rel="urn:ietf:params:push", ' +
        '</receiptPushEndpointLocationBogus>; rel="urn:ietf:params:push:receipt"'
    );
    res.writeHead(201, "OK");
    res.end("");
    return;
  } else if (u.pathname === "/pushSubscriptionMissingLink1/subscribe") {
    res.setHeader(
      "Location",
      "https://localhost:" + serverPort + "/subscriptionMissingLink1"
    );
    res.setHeader(
      "Link",
      '</receiptPushEndpointMissingLink1>; rel="urn:ietf:params:push:receipt"'
    );
    res.writeHead(201, "OK");
    res.end("");
    return;
  } else if (u.pathname === "/pushSubscriptionMissingLink2/subscribe") {
    res.setHeader(
      "Location",
      "https://localhost:" + serverPort + "/subscriptionMissingLink2"
    );
    res.setHeader(
      "Link",
      '</pushEndpointMissingLink2>; rel="urn:ietf:params:push"'
    );
    res.writeHead(201, "OK");
    res.end("");
    return;
  } else if (u.pathname === "/subscriptionMissingLink2") {
    // do nothing.
    return;
  } else if (u.pathname === "/pushSubscriptionNot201Code/subscribe") {
    res.setHeader(
      "Location",
      "https://localhost:" + serverPort + "/subscriptionNot2xxCode"
    );
    res.setHeader(
      "Link",
      '</pushEndpointNot201Code>; rel="urn:ietf:params:push", ' +
        '</receiptPushEndpointNot201Code>; rel="urn:ietf:params:push:receipt"'
    );
    res.writeHead(200, "OK");
    res.end("");
    return;
  } else if (u.pathname === "/pushNotifications/subscription1") {
    pushPushServer1 = res.push({
      hostname: "localhost:" + serverPort,
      port: serverPort,
      path: "/pushNotificationsDeliver1",
      method: "GET",
      headers: {
        "Encryption-Key":
          'keyid="notification1"; dh="BO_tgGm-yvYAGLeRe16AvhzaUcpYRiqgsGOlXpt0DRWDRGGdzVLGlEVJMygqAUECarLnxCiAOHTP_znkedrlWoU"',
        Encryption: 'keyid="notification1";salt="uAZaiXpOSfOLJxtOCZ09dA"',
        "Content-Encoding": "aesgcm128",
      },
    });
    pushPushServer1.writeHead(200, {
      subresource: "1",
    });

    pushPushServer1.end(
      "370aeb3963f12c4f12bf946bd0a7a9ee7d3eaff8f7aec62b530fc25cfa",
      "hex"
    );
    return;
  } else if (u.pathname === "/pushNotifications/subscription2") {
    pushPushServer2 = res.push({
      hostname: "localhost:" + serverPort,
      port: serverPort,
      path: "/pushNotificationsDeliver3",
      method: "GET",
      headers: {
        "Encryption-Key":
          'keyid="notification2"; dh="BKVdQcgfncpNyNWsGrbecX0zq3eHIlHu5XbCGmVcxPnRSbhjrA6GyBIeGdqsUL69j5Z2CvbZd-9z1UBH0akUnGQ"',
        Encryption: 'keyid="notification2";salt="vFn3t3M_k42zHBdpch3VRw"',
        "Content-Encoding": "aesgcm128",
      },
    });
    pushPushServer2.writeHead(200, {
      subresource: "1",
    });

    pushPushServer2.end(
      "66df5d11daa01e5c802ff97cdf7f39684b5bf7c6418a5cf9b609c6826c04b25e403823607ac514278a7da945",
      "hex"
    );
    return;
  } else if (u.pathname === "/pushNotifications/subscription3") {
    pushPushServer3 = res.push({
      hostname: "localhost:" + serverPort,
      port: serverPort,
      path: "/pushNotificationsDeliver3",
      method: "GET",
      headers: {
        "Encryption-Key":
          'keyid="notification3";dh="BD3xV_ACT8r6hdIYES3BJj1qhz9wyv7MBrG9vM2UCnjPzwE_YFVpkD-SGqE-BR2--0M-Yf31wctwNsO1qjBUeMg"',
        Encryption:
          'keyid="notification3"; salt="DFq188piWU7osPBgqn4Nlg"; rs=24',
        "Content-Encoding": "aesgcm128",
      },
    });
    pushPushServer3.writeHead(200, {
      subresource: "1",
    });

    pushPushServer3.end(
      "2caaeedd9cf1059b80c58b6c6827da8ff7de864ac8bea6d5775892c27c005209cbf9c4de0c3fbcddb9711d74eaeebd33f7275374cb42dd48c07168bc2cc9df63e045ce2d2a2408c66088a40c",
      "hex"
    );
    return;
  } else if (u.pathname == "/pushNotifications/subscription4") {
    pushPushServer4 = res.push({
      hostname: "localhost:" + serverPort,
      port: serverPort,
      path: "/pushNotificationsDeliver4",
      method: "GET",
      headers: {
        "Crypto-Key":
          'keyid="notification4";dh="BJScXUUTcs7D8jJWI1AOxSgAKkF7e56ay4Lek52TqDlWo1yGd5czaxFWfsuP4j7XNWgGYm60-LKpSUMlptxPFVQ"',
        Encryption: 'keyid="notification4"; salt="sn9p2QqF3V6KBclda8vx7w"',
        "Content-Encoding": "aesgcm",
      },
    });
    pushPushServer4.writeHead(200, {
      subresource: "1",
    });

    pushPushServer4.end(
      "9eba7ba6192544a39bd9e9b58e702d0748f1776b27f6616cdc55d29ed5a015a6db8f2dd82cd5751a14315546194ff1c18458ab91eb36c9760ccb042670001fd9964557a079553c3591ee131ceb259389cfffab3ab873f873caa6a72e87d262b8684c3260e5940b992234deebf57a9ff3a8775742f3cbcb152d249725a28326717e19cce8506813a155eff5df9bdba9e3ae8801d3cc2b7e7f2f1b6896e63d1fdda6f85df704b1a34db7b2dd63eba11ede154300a318c6f83c41a3d32356a196e36bc905b99195fd91ae4ff3f545c42d17f1fdc1d5bd2bf7516d0765e3a859fffac84f46160b79cedda589f74c25357cf6988cd8ba83867ebd86e4579c9d3b00a712c77fcea3b663007076e21f9819423faa830c2176ff1001c1690f34be26229a191a938517",
      "hex"
    );
    return;
  } else if (
    u.pathname === "/pushNotificationsDeliver1" ||
    u.pathname === "/pushNotificationsDeliver2" ||
    u.pathname === "/pushNotificationsDeliver3"
  ) {
    res.writeHead(410, "GONE");
    res.end("");
    return;
  } else if (u.pathname === "/illegalhpacksoft") {
    // This will cause the compressor to compress a header that is not legal,
    // but only affects the stream, not the session.
    illegalheader_conn = req.stream.connection;
    Compressor.prototype.compress = insertSoftIllegalHpack;
    // Fall through to the default response behavior
  } else if (u.pathname === "/illegalhpackhard") {
    // This will cause the compressor to insert an HPACK instruction that will
    // cause a session failure.
    Compressor.prototype.compress = insertHardIllegalHpack;
    // Fall through to default response behavior
  } else if (u.pathname === "/illegalhpack_validate") {
    if (req.stream.connection === illegalheader_conn) {
      res.setHeader("X-Did-Goaway", "no");
    } else {
      res.setHeader("X-Did-Goaway", "yes");
    }
    // Fall through to the default response behavior
  } else if (u.pathname === "/foldedheader") {
    res.setHeader("X-Folded-Header", "this is\n folded");
    // Fall through to the default response behavior
  } else if (u.pathname === "/emptydata") {
    // Overwrite the original transform with our version that will insert an
    // empty DATA frame at the beginning of the stream response, then fall
    // through to the default response behavior.
    Serializer.prototype._transform = newTransform;
  }

  // for use with test_immutable.js
  else if (u.pathname === "/immutable-test-without-attribute") {
    res.setHeader("Cache-Control", "max-age=100000");
    res.setHeader("Etag", "1");
    if (req.headers["if-none-match"]) {
      res.setHeader("x-conditional", "true");
    }
    // default response from here
  } else if (u.pathname === "/immutable-test-with-attribute") {
    res.setHeader("Cache-Control", "max-age=100000, immutable");
    res.setHeader("Etag", "2");
    if (req.headers["if-none-match"]) {
      res.setHeader("x-conditional", "true");
    }
    // default response from here
  } else if (u.pathname === "/origin-4") {
    let originList = [];
    req.stream.connection.originFrame(originList);
    res.setHeader("x-client-port", req.remotePort);
  } else if (u.pathname === "/origin-6") {
    let originList = [
      "https://alt1.example.com:" + serverPort,
      "https://alt2.example.com:" + serverPort,
      "https://bar.example.com:" + serverPort,
    ];
    req.stream.connection.originFrame(originList);
    res.setHeader("x-client-port", req.remotePort);
  } else if (u.pathname === "/origin-11-a") {
    res.setHeader("x-client-port", req.remotePort);

    const pushb = res.push({
      hostname: "foo.example.com:" + serverPort,
      port: serverPort,
      path: "/origin-11-b",
      method: "GET",
      headers: { "x-pushed-request": "true", "x-foo": "bar" },
    });
    pushb.writeHead(200, {
      pushed: "yes",
      "content-length": 1,
    });
    pushb.end("1");

    const pushc = res.push({
      hostname: "bar.example.com:" + serverPort,
      port: serverPort,
      path: "/origin-11-c",
      method: "GET",
      headers: { "x-pushed-request": "true", "x-foo": "bar" },
    });
    pushc.writeHead(200, {
      pushed: "yes",
      "content-length": 1,
    });
    pushc.end("1");

    const pushd = res.push({
      hostname: "madeup.example.com:" + serverPort,
      port: serverPort,
      path: "/origin-11-d",
      method: "GET",
      headers: { "x-pushed-request": "true", "x-foo": "bar" },
    });
    pushd.writeHead(200, {
      pushed: "yes",
      "content-length": 1,
    });
    pushd.end("1");

    const pushe = res.push({
      hostname: "alt1.example.com:" + serverPort,
      port: serverPort,
      path: "/origin-11-e",
      method: "GET",
      headers: { "x-pushed-request": "true", "x-foo": "bar" },
    });
    pushe.writeHead(200, {
      pushed: "yes",
      "content-length": 1,
    });
    pushe.end("1");
  } else if (u.pathname.substring(0, 8) === "/origin-") {
    // test_origin.js coalescing
    res.setHeader("x-client-port", req.remotePort);
  } else if (u.pathname === "/statusphrase") {
    // Fortunately, the node-http2 API is dumb enough to allow this right on
    // through, so we can easily test rejecting this on gecko's end.
    res.writeHead("200 OK");
    res.end(content);
    return;
  } else if (u.pathname === "/doublepush") {
    push1 = res.push("/doublypushed");
    push1.writeHead(200, {
      "content-type": "text/plain",
      pushed: "yes",
      "content-length": 6,
      "X-Connection-Http2": "yes",
    });
    push1.end("pushed");

    push2 = res.push("/doublypushed");
    push2.writeHead(200, {
      "content-type": "text/plain",
      pushed: "yes",
      "content-length": 6,
      "X-Connection-Http2": "yes",
    });
    push2.end("pushed");
  } else if (u.pathname === "/doublypushed") {
    content = "not pushed";
  } else if (u.pathname === "/diskcache") {
    content = "this was pulled via h2";
  } else if (u.pathname === "/pushindisk") {
    var pushedContent = "this was pushed via h2";
    push = res.push("/diskcache");
    push.writeHead(200, {
      "content-type": "text/html",
      pushed: "yes",
      "content-length": pushedContent.length,
      "X-Connection-Http2": "yes",
    });
    push.end(pushedContent);
  }

  // For test_header_Server_Timing.js
  else if (u.pathname === "/server-timing") {
    res.setHeader("Content-Type", "text/plain");
    res.setHeader("Content-Length", "12");
    res.setHeader("Trailer", "Server-Timing");
    res.setHeader(
      "Server-Timing",
      "metric; dur=123.4; desc=description, metric2; dur=456.78; desc=description1"
    );
    res.write("data reached");
    res.addTrailers({
      "Server-Timing":
        "metric3; dur=789.11; desc=description2, metric4; dur=1112.13; desc=description3",
    });
    res.end();
    return;
  }

  res.setHeader("Content-Type", "text/html");
  if (req.httpVersionMajor != 2) {
    res.setHeader("Connection", "close");
  }
  res.writeHead(200);
  res.end(content);
}

// Set up the SSL certs for our server - this server has a cert for foo.example.com
// signed by netwerk/tests/unit/http2-ca.pem
var options = {
  key: fs.readFileSync(__dirname + "/http2-cert.key"),
  cert: fs.readFileSync(__dirname + "/http2-cert.pem"),
};

if (process.env.HTTP2_LOG !== undefined) {
  var log_module = node_http2_root + "/test/util";
  options.log = require(log_module).createLogger("server");
}

var server = http2.createServer(options, handleRequest);

server.on("connection", function(socket) {
  socket.on("error", function() {
    // Ignoring SSL socket errors, since they usually represent a connection that was tore down
    // by the browser because of an untrusted certificate. And this happens at least once, when
    // the first test case if done.
  });
});

function makeid(length) {
  var result = "";
  var characters =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  var charactersLength = characters.length;
  for (var i = 0; i < length; i++) {
    result += characters.charAt(Math.floor(Math.random() * charactersLength));
  }
  return result;
}

let globalObjects = {};
var serverPort;

const listen = (serv, envport) => {
  if (!serv) {
    return Promise.resolve(0);
  }

  let portSelection = 0;
  if (envport !== undefined) {
    try {
      portSelection = parseInt(envport, 10);
    } catch (e) {
      portSelection = -1;
    }
  }
  return new Promise(resolve => {
    serv.listen(portSelection, "0.0.0.0", 2000, () => {
      resolve(serv.address().port);
    });
  });
};

const http = require("http");
let httpServer = http.createServer((req, res) => {
  if (req.method != "POST") {
    let u = url.parse(req.url, true);
    if (u.pathname == "/test") {
      // This path is used to test that the server is working properly
      res.writeHead(200);
      res.end("OK");
      return;
    }
    res.writeHead(405);
    res.end("Unexpected method: " + req.method);
    return;
  }

  let code = "";
  req.on("data", function receivePostData(chunk) {
    code += chunk;
  });
  req.on("end", function finishPost() {
    let u = url.parse(req.url, true);
    if (u.pathname == "/fork") {
      let id = forkProcess();
      computeAndSendBackResponse(id);
      return;
    }

    if (u.pathname.startsWith("/kill/")) {
      let id = u.pathname.slice(6);
      let forked = globalObjects[id];
      if (!forked) {
        computeAndSendBackResponse(undefined, new Error("could not find id"));
        return;
      }

      new Promise((resolve, reject) => {
        forked.resolve = resolve;
        forked.reject = reject;
        forked.kill();
      })
        .then(x =>
          computeAndSendBackResponse(
            undefined,
            new Error(`incorrectly resolved ${x}`)
          )
        )
        .catch(e => {
          // We indicate a proper shutdown by resolving with undefined.
          if (e && e.toString().match(/child process exit closing code/)) {
            e = undefined;
          }
          computeAndSendBackResponse(undefined, e);
        });
      return;
    }

    if (u.pathname.startsWith("/execute/")) {
      let id = u.pathname.slice(9);
      let forked = globalObjects[id];
      if (!forked) {
        computeAndSendBackResponse(undefined, new Error("could not find id"));
        return;
      }

      new Promise((resolve, reject) => {
        forked.resolve = resolve;
        forked.reject = reject;
        forked.send({ code });
      })
        .then(x => sendBackResponse(x))
        .catch(e => computeAndSendBackResponse(undefined, e));
    }

    function computeAndSendBackResponse(evalResult, e) {
      let output = { result: evalResult, error: "", errorStack: "" };
      if (e) {
        output.error = e.toString();
        output.errorStack = e.stack;
      }
      sendBackResponse(output);
    }

    function sendBackResponse(output) {
      output = JSON.stringify(output);

      res.setHeader("Content-Length", output.length);
      res.setHeader("Content-Type", "application/json");
      res.writeHead(200);
      res.write(output);
      res.end("");
    }
  });
});

function forkProcess() {
  let scriptPath = path.resolve(__dirname, "moz-http2-child.js");
  let id = makeid(6);
  let forked = fork(scriptPath);
  forked.errors = "";
  globalObjects[id] = forked;
  forked.on("message", msg => {
    if (forked.resolve) {
      forked.resolve(msg);
      forked.resolve = null;
    } else {
      console.log(`forked process without handler sent: ${msg}`);
      forked.errors += `forked process without handler sent: ${JSON.stringify(
        msg
      )}\n`;
    }
  });

  let exitFunction = (code, signal) => {
    if (globalObjects[id]) {
      delete globalObjects[id];
    } else {
      // already called
      return;
    }

    if (!forked.reject) {
      console.log(
        `child process ${id} closing code: ${code} signal: ${signal}`
      );
      return;
    }

    if (forked.errors != "") {
      forked.reject(forked.errors);
      forked.errors = "";
      forked.reject = null;
      return;
    }

    forked.reject(`child process exit closing code: ${code} signal: ${signal}`);
    forked.reject = null;
  };

  forked.on("error", exitFunction);
  forked.on("close", exitFunction);
  forked.on("exit", exitFunction);

  return id;
}

Promise.all([
  listen(server, process.env.MOZHTTP2_PORT).then(port => (serverPort = port)),
  listen(httpServer, process.env.MOZNODE_EXEC_PORT),
]).then(([sPort, nodeExecPort]) => {
  console.log(`HTTP2 server listening on ports ${sPort},${nodeExecPort}`);
});
