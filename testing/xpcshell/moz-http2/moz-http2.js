/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This module is the stateful server side of test_http2.js and is meant
// to have node be restarted in between each invocation

var node_http2_root = '../node-http2';
if (process.env.NODE_HTTP2_ROOT) {
  node_http2_root = process.env.NODE_HTTP2_ROOT;
}
var http2 = require(node_http2_root);
var fs = require('fs');
var url = require('url');
var crypto = require('crypto');

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
}

var connection_module = node_http2_root + "/lib/protocol/connection";
var http2_connection = require(connection_module);
var Connection = http2_connection.Connection;
var originalClose = Connection.prototype.close;
Connection.prototype.close = function (error, lastId) {
  if (lastId !== undefined) {
    this._lastIncomingStream = lastId;
  }

  originalClose.apply(this, arguments);
}

var framer_module = node_http2_root + "/lib/protocol/framer";
var http2_framer = require(framer_module);
var Serializer = http2_framer.Serializer;
var originalTransform = Serializer.prototype._transform;
var newTransform = function (frame, encoding, done) {
  if (frame.type == 'DATA') {
    // Insert our empty DATA frame
    emptyFrame = {};
    emptyFrame.type = 'DATA';
    emptyFrame.data = new Buffer(0);
    emptyFrame.flags = [];
    emptyFrame.stream = frame.stream;
    var buffers = [];
    Serializer['DATA'](emptyFrame, buffers);
    Serializer.commonHeader(emptyFrame, buffers);
    for (var i = 0; i < buffers.length; i++) {
      this.push(buffers[i]);
    }

    // Reset to the original version for later uses
    Serializer.prototype._transform = originalTransform;
  }
  originalTransform.apply(this, arguments);
};

function getHttpContent(path) {
  var content = '<!doctype html>' +
                '<html>' +
                '<head><title>HOORAY!</title></head>' +
                '<body>You Win! (by requesting' + path + ')</body>' +
                '</html>';
  return content;
}

function generateContent(size) {
  var content = '';
  for (var i = 0; i < size; i++) {
    content += '0';
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

  checkReady: function() {
    if (this.mp1res != null && this.mp2res != null) {
      this.buf = generateContent(30*1024);
      this.mp1start = 0;
      this.mp2start = 0;
      this.send(this.mp1res, 0);
      setTimeout(this.send.bind(this, this.mp2res, 0), 5);
    }
  },

  send: function(res, start) {
    var end = Math.min(start + 1024, this.buf.length);
    var content = this.buf.substring(start, end);
    res.write(content);
    if (end < this.buf.length) {
      setTimeout(this.send.bind(this, res, end), 10);
    } else {
      res.end();
    }
  }
};

var runlater = function() {};
runlater.prototype = {
  req : null,
  resp : null,

  onTimeout : function onTimeout() {
    this.resp.writeHead(200);
    this.resp.end("It's all good 750ms.");
  }
};

var moreData = function() {};
moreData.prototype = {
  req : null,
  resp : null,
  iter: 3,

  onTimeout : function onTimeout() {
    // 1mb of data
    content = generateContent(1024*1024);
    this.resp.write(content); // 1mb chunk
    this.iter--;
    if (!this.iter) {
      this.resp.end();
    } else {
      setTimeout(executeRunLater, 1, this);
    }
  }
};

function executeRunLater(arg) {
  arg.onTimeout();
}

var Compressor = http2_compression.Compressor;
var HeaderSetCompressor = http2_compression.HeaderSetCompressor;
var originalCompressHeaders = Compressor.prototype.compress;

function insertSoftIllegalHpack(headers) {
  var originalCompressed = originalCompressHeaders.apply(this, headers);
  var illegalLiteral = new Buffer([
      0x00, // Literal, no index
      0x08, // Name: not huffman encoded, 8 bytes long
      0x3a, 0x69, 0x6c, 0x6c, 0x65, 0x67, 0x61, 0x6c, // :illegal
      0x10, // Value: not huffman encoded, 16 bytes long
      // REALLY NOT LEGAL
      0x52, 0x45, 0x41, 0x4c, 0x4c, 0x59, 0x20, 0x4e, 0x4f, 0x54, 0x20, 0x4c, 0x45, 0x47, 0x41, 0x4c
  ]);
  var newBufferLength = originalCompressed.length + illegalLiteral.length;
  var concatenated = new Buffer(newBufferLength);
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
  var concatenated = new Buffer(newBufferLength);
  originalCompressed.copy(concatenated, 0);
  illegalIndexed.copy(concatenated, originalCompressed.length);
  return concatenated;
}

var h11required_conn = null;
var h11required_header = "yes";
var didRst = false;
var rstConnection = null;
var illegalheader_conn = null;

function handleRequest(req, res) {
  // We do this first to ensure nothing goes wonky in our tests that don't want
  // the headers to have something illegal in them
  Compressor.prototype.compress = originalCompressHeaders;

  var u = url.parse(req.url);
  var content = getHttpContent(u.pathname);
  var push, push1, push1a, push2, push3;

  // PushService tests.
  var pushPushServer1, pushPushServer2, pushPushServer3, pushPushServer4;

  if (req.httpVersionMajor === 2) {
    res.setHeader('X-Connection-Http2', 'yes');
    res.setHeader('X-Http2-StreamId', '' + req.stream.id);
  } else {
    res.setHeader('X-Connection-Http2', 'no');
  }

  if (u.pathname === '/exit') {
    res.setHeader('Content-Type', 'text/plain');
    res.setHeader('Connection', 'close');
    res.writeHead(200);
    res.end('ok');
    process.exit();
  }

  if (u.pathname === '/750ms') {
    var rl = new runlater();
    rl.req = req;
    rl.resp = res;
    setTimeout(executeRunLater, 750, rl);
    return;
  }

  else if ((u.pathname === '/multiplex1') && (req.httpVersionMajor === 2)) {
    res.setHeader('Content-Type', 'text/plain');
    res.writeHead(200);
    m.mp1res = res;
    m.checkReady();
    return;
  }

  else if ((u.pathname === '/multiplex2') && (req.httpVersionMajor === 2)) {
    res.setHeader('Content-Type', 'text/plain');
    res.writeHead(200);
    m.mp2res = res;
    m.checkReady();
    return;
  }

  else if (u.pathname === "/header") {
    var val = req.headers["x-test-header"];
    if (val) {
      res.setHeader("X-Received-Test-Header", val);
    }
  }

  else if (u.pathname === "/doubleheader") {
    res.setHeader('Content-Type', 'text/html');
    res.writeHead(200);
    res.write(content);
    res.writeHead(200);
    res.end();
    return;
  }

  else if (u.pathname === "/cookie_crumbling") {
    res.setHeader("X-Received-Header-Pairs", JSON.stringify(decompressedPairs));
  }

  else if (u.pathname === "/push") {
    push = res.push('/push.js');
    push.writeHead(200, {
      'content-type': 'application/javascript',
      'pushed' : 'yes',
      'content-length' : 11,
      'X-Connection-Http2': 'yes'
    });
    push.end('// comments');
    content = '<head> <script src="push.js"/></head>body text';
  }

  else if (u.pathname === "/push2") {
    push = res.push('/push2.js');
    push.writeHead(200, {
      'content-type': 'application/javascript',
      'pushed' : 'yes',
      // no content-length
      'X-Connection-Http2': 'yes'
    });
    push.end('// comments');
    content = '<head> <script src="push2.js"/></head>body text';
  }

  else if (u.pathname === "/push5") {
    push = res.push('/push5.js');
    push.writeHead(200, {
      'content-type': 'application/javascript',
      'pushed' : 'yes',
      // no content-length
      'X-Connection-Http2': 'yes'
    });
    content = generateContent(1024 * 150);
    push.write(content);
    push.end();
    content = '<head> <script src="push5.js"/></head>body text';
  }

  else if (u.pathname === "/pushapi1") {
    push1 = res.push(
        { hostname: 'localhost:' + serverPort, port: serverPort, path : '/pushapi1/1', method : 'GET',
          headers: {'x-pushed-request': 'true', 'x-foo' : 'bar'}});
    push1.writeHead(200, {
      'pushed' : 'yes',
      'content-length' : 1,
      'subresource' : '1',
      'X-Connection-Http2': 'yes'
      });
    push1.end('1');

    push1a = res.push(
        { hostname: 'localhost:' + serverPort, port: serverPort, path : '/pushapi1/1', method : 'GET',
          headers: {'x-foo' : 'bar', 'x-pushed-request': 'true'}});
    push1a.writeHead(200, {
      'pushed' : 'yes',
      'content-length' : 1,
      'subresource' : '1a',
      'X-Connection-Http2': 'yes'
    });
    push1a.end('1');

    push2 = res.push(
        { hostname: 'localhost:' + serverPort, port: serverPort, path : '/pushapi1/2', method : 'GET',
          headers: {'x-pushed-request': 'true'}});
    push2.writeHead(200, {
      'pushed' : 'yes',
      'subresource' : '2',
      'content-length' : 1,
      'X-Connection-Http2': 'yes'
    });
    push2.end('2');

    push3 = res.push(
        { hostname: 'localhost:' + serverPort, port: serverPort, path : '/pushapi1/3', method : 'GET',
          headers: {'x-pushed-request': 'true'}});
    push3.writeHead(200, {
      'pushed' : 'yes',
      'content-length' : 1,
      'subresource' : '3',
      'X-Connection-Http2': 'yes'
    });
    push3.end('3');

    content = '0';
  }

  else if (u.pathname === "/big") {
    content = generateContent(128 * 1024);
    var hash = crypto.createHash('md5');
    hash.update(content);
    var md5 = hash.digest('hex');
    res.setHeader("X-Expected-MD5", md5);
  }

  else if (u.pathname === "/huge") {
    content = generateContent(1024);
    res.setHeader('Content-Type', 'text/plain');
    res.writeHead(200);
    // 1mb of data
    for (var i = 0; i < (1024 * 1); i++) {
      res.write(content); // 1kb chunk
    }
    res.end();
    return;
  }

  else if (u.pathname === "/post" || u.pathname === "/patch") {
    if (req.method != "POST" && req.method != "PATCH") {
      res.writeHead(405);
      res.end('Unexpected method: ' + req.method);
      return;
    }

    var post_hash = crypto.createHash('md5');
    req.on('data', function receivePostData(chunk) {
      post_hash.update(chunk.toString());
    });
    req.on('end', function finishPost() {
      var md5 = post_hash.digest('hex');
      res.setHeader('X-Calculated-MD5', md5);
      res.writeHead(200);
      res.end(content);
    });

    return;
  }

  else if (u.pathname === "/750msPost") {
    if (req.method != "POST") {
      res.writeHead(405);
      res.end('Unexpected method: ' + req.method);
      return;
    }

    var accum = 0;
    req.on('data', function receivePostData(chunk) {
      accum += chunk.length;
    });
    req.on('end', function finishPost() {
      res.setHeader('X-Recvd', accum);
      var rl = new runlater();
      rl.req = req;
      rl.resp = res;
      setTimeout(executeRunLater, 750, rl);
      return;
    });

    return;
  }

  else if (u.pathname === "/h11required_stream") {
    if (req.httpVersionMajor === 2) {
      h11required_conn = req.stream.connection;
      res.stream.reset('HTTP_1_1_REQUIRED');
      return;
    }
  }

  else if (u.pathname === "/bigdownload") {

    res.setHeader('Content-Type', 'text/html');
    res.writeHead(200);

    var rl = new moreData();
    rl.req = req;
    rl.resp = res;
    setTimeout(executeRunLater, 1, rl);
    return;
  }

  else if (u.pathname === "/h11required_session") {
    if (req.httpVersionMajor === 2) {
      if (h11required_conn !== req.stream.connection) {
        h11required_header = "no";
      }
      res.stream.connection.close('HTTP_1_1_REQUIRED', res.stream.id - 2);
      return;
    } else {
      res.setHeader('X-H11Required-Stream-Ok', h11required_header);
    }
  }

  else if (u.pathname === "/rstonce") {
    if (!didRst && req.httpVersionMajor === 2) {
      didRst = true;
      rstConnection = req.stream.connection;
      req.stream.reset('REFUSED_STREAM');
      return;
    }

    if (rstConnection === null ||
        rstConnection !== req.stream.connection) {
      res.setHeader('Connection', 'close');
      res.writeHead(400);
      res.end("WRONG CONNECTION, HOMIE!");
      return;
    }

    if (req.httpVersionMajor != 2) {
      res.setHeader('Connection', 'close');
    }
    res.writeHead(200);
    res.end("It's all good.");
    return;
  }

  else if (u.pathname === "/continuedheaders") {
    var pushRequestHeaders = {'x-pushed-request': 'true'};
    var pushResponseHeaders = {'content-type': 'text/plain',
                               'content-length': '2',
                               'X-Connection-Http2': 'yes'};
    var pushHdrTxt = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789';
    var pullHdrTxt = pushHdrTxt.split('').reverse().join('');
    for (var i = 0; i < 265; i++) {
      pushRequestHeaders['X-Push-Test-Header-' + i] = pushHdrTxt;
      res.setHeader('X-Pull-Test-Header-' + i, pullHdrTxt);
    }
    push = res.push({
      hostname: 'localhost:' + serverPort,
      port: serverPort,
      path: '/continuedheaders/push',
      method: 'GET',
      headers: pushRequestHeaders
    });
    push.writeHead(200, pushResponseHeaders);
    push.end("ok");
  }

  else if (u.pathname === "/altsvc1") {
    if (req.httpVersionMajor != 2 ||
      req.scheme != "http" ||
      req.headers['alt-used'] != ("localhost:" + serverPort)) {
      res.setHeader('Connection', 'close');
      res.writeHead(400);
      res.end("WHAT?");
      return;
   }
   // test the alt svc frame for use with altsvc2
   res.altsvc("localhost", serverPort, "h2", 3600, req.headers['x-redirect-origin']);
  }

  else if (u.pathname === "/altsvc2") {
    if (req.httpVersionMajor != 2 ||
      req.scheme != "http" ||
      req.headers['alt-used'] != ("localhost:" + serverPort)) {
      res.setHeader('Connection', 'close');
      res.writeHead(400);
      res.end("WHAT?");
      return;
   }
  }

  // for use with test_altsvc.js
  else if (u.pathname === "/altsvc-test") {
    res.setHeader('Cache-Control', 'no-cache');
    res.setHeader('Alt-Svc', 'h2=' + req.headers['x-altsvc']);
  }

  // for PushService tests.
  else if (u.pathname === "/pushSubscriptionSuccess/subscribe") {
    res.setHeader("Location",
                  'https://localhost:' + serverPort + '/pushSubscriptionSuccesss');
    res.setHeader("Link",
                  '</pushEndpointSuccess>; rel="urn:ietf:params:push", ' +
                  '</receiptPushEndpointSuccess>; rel="urn:ietf:params:push:receipt"');
    res.writeHead(201, "OK");
    res.end("");
    return;
  }

  else if (u.pathname === "/pushSubscriptionSuccesss") {
    // do nothing.
    return;
  }

  else if (u.pathname === "/pushSubscriptionMissingLocation/subscribe") {
    res.setHeader("Link",
                  '</pushEndpointMissingLocation>; rel="urn:ietf:params:push", ' +
                  '</receiptPushEndpointMissingLocation>; rel="urn:ietf:params:push:receipt"');
    res.writeHead(201, "OK");
    res.end("");
    return;
  }

  else if (u.pathname === "/pushSubscriptionMissingLink/subscribe") {
    res.setHeader("Location",
                  'https://localhost:' + serverPort + '/subscriptionMissingLink');
    res.writeHead(201, "OK");
    res.end("");
    return;
  }

  else if (u.pathname === "/pushSubscriptionLocationBogus/subscribe") {
    res.setHeader("Location", '1234');
    res.setHeader("Link",
                  '</pushEndpointLocationBogus; rel="urn:ietf:params:push", ' +
                  '</receiptPushEndpointLocationBogus>; rel="urn:ietf:params:push:receipt"');
    res.writeHead(201, "OK");
    res.end("");
    return;
  }

  else if (u.pathname === "/pushSubscriptionMissingLink1/subscribe") {
    res.setHeader("Location",
                  'https://localhost:' + serverPort + '/subscriptionMissingLink1');
    res.setHeader("Link",
                  '</receiptPushEndpointMissingLink1>; rel="urn:ietf:params:push:receipt"');
    res.writeHead(201, "OK");
    res.end("");
    return;
  }

  else if (u.pathname === "/pushSubscriptionMissingLink2/subscribe") {
    res.setHeader("Location",
                  'https://localhost:' + serverPort + '/subscriptionMissingLink2');
    res.setHeader("Link",
                  '</pushEndpointMissingLink2>; rel="urn:ietf:params:push"');
    res.writeHead(201, "OK");
    res.end("");
    return;
  }

  else if (u.pathname === "/subscriptionMissingLink2") {
    // do nothing.
    return;
  }

  else if (u.pathname === "/pushSubscriptionNot201Code/subscribe") {
    res.setHeader("Location",
                  'https://localhost:' + serverPort + '/subscriptionNot2xxCode');
    res.setHeader("Link",
                  '</pushEndpointNot201Code>; rel="urn:ietf:params:push", ' +
                  '</receiptPushEndpointNot201Code>; rel="urn:ietf:params:push:receipt"');
    res.writeHead(200, "OK");
    res.end("");
    return;
  }

  else if (u.pathname ==="/pushNotifications/subscription1") {
    pushPushServer1 = res.push(
      { hostname: 'localhost:' + serverPort, port: serverPort,
        path : '/pushNotificationsDeliver1', method : 'GET',
        headers: { 'Encryption-Key': 'keyid="notification1"; dh="BO_tgGm-yvYAGLeRe16AvhzaUcpYRiqgsGOlXpt0DRWDRGGdzVLGlEVJMygqAUECarLnxCiAOHTP_znkedrlWoU"',
                   'Encryption': 'keyid="notification1";salt="uAZaiXpOSfOLJxtOCZ09dA"',
                   'Content-Encoding': 'aesgcm128',
                 }
      });
    pushPushServer1.writeHead(200, {
      'subresource' : '1'
      });

    pushPushServer1.end('370aeb3963f12c4f12bf946bd0a7a9ee7d3eaff8f7aec62b530fc25cfa', 'hex');
    return;
  }

  else if (u.pathname ==="/pushNotifications/subscription2") {
    pushPushServer2 = res.push(
      { hostname: 'localhost:' + serverPort, port: serverPort,
        path : '/pushNotificationsDeliver3', method : 'GET',
        headers: { 'Encryption-Key': 'keyid="notification2"; dh="BKVdQcgfncpNyNWsGrbecX0zq3eHIlHu5XbCGmVcxPnRSbhjrA6GyBIeGdqsUL69j5Z2CvbZd-9z1UBH0akUnGQ"',
                   'Encryption': 'keyid="notification2";salt="vFn3t3M_k42zHBdpch3VRw"',
                   'Content-Encoding': 'aesgcm128',
                 }
      });
    pushPushServer2.writeHead(200, {
      'subresource' : '1'
      });

    pushPushServer2.end('66df5d11daa01e5c802ff97cdf7f39684b5bf7c6418a5cf9b609c6826c04b25e403823607ac514278a7da945', 'hex');
    return;
  }

  else if (u.pathname ==="/pushNotifications/subscription3") {
    pushPushServer3 = res.push(
      { hostname: 'localhost:' + serverPort, port: serverPort,
        path : '/pushNotificationsDeliver3', method : 'GET',
        headers: { 'Encryption-Key': 'keyid="notification3";dh="BD3xV_ACT8r6hdIYES3BJj1qhz9wyv7MBrG9vM2UCnjPzwE_YFVpkD-SGqE-BR2--0M-Yf31wctwNsO1qjBUeMg"',
                   'Encryption': 'keyid="notification3"; salt="DFq188piWU7osPBgqn4Nlg"; rs=24',
                   'Content-Encoding': 'aesgcm128',
                 }
      });
    pushPushServer3.writeHead(200, {
      'subresource' : '1'
      });

    pushPushServer3.end('2caaeedd9cf1059b80c58b6c6827da8ff7de864ac8bea6d5775892c27c005209cbf9c4de0c3fbcddb9711d74eaeebd33f7275374cb42dd48c07168bc2cc9df63e045ce2d2a2408c66088a40c', 'hex');
    return;
  }

  else if (u.pathname == "/pushNotifications/subscription4") {
    pushPushServer4 = res.push(
      { hostname: 'localhost:' + serverPort, port: serverPort,
        path : '/pushNotificationsDeliver4', method : 'GET',
        headers: { 'Crypto-Key': 'keyid="notification4";dh="BJScXUUTcs7D8jJWI1AOxSgAKkF7e56ay4Lek52TqDlWo1yGd5czaxFWfsuP4j7XNWgGYm60-LKpSUMlptxPFVQ"',
                   'Encryption': 'keyid="notification4"; salt="sn9p2QqF3V6KBclda8vx7w"',
                   'Content-Encoding': 'aesgcm',
                 }
      });
    pushPushServer4.writeHead(200, {
      'subresource' : '1'
      });

    pushPushServer4.end('9eba7ba6192544a39bd9e9b58e702d0748f1776b27f6616cdc55d29ed5a015a6db8f2dd82cd5751a14315546194ff1c18458ab91eb36c9760ccb042670001fd9964557a079553c3591ee131ceb259389cfffab3ab873f873caa6a72e87d262b8684c3260e5940b992234deebf57a9ff3a8775742f3cbcb152d249725a28326717e19cce8506813a155eff5df9bdba9e3ae8801d3cc2b7e7f2f1b6896e63d1fdda6f85df704b1a34db7b2dd63eba11ede154300a318c6f83c41a3d32356a196e36bc905b99195fd91ae4ff3f545c42d17f1fdc1d5bd2bf7516d0765e3a859fffac84f46160b79cedda589f74c25357cf6988cd8ba83867ebd86e4579c9d3b00a712c77fcea3b663007076e21f9819423faa830c2176ff1001c1690f34be26229a191a938517', 'hex');
    return;
  }

  else if ((u.pathname === "/pushNotificationsDeliver1") ||
           (u.pathname === "/pushNotificationsDeliver2") ||
           (u.pathname === "/pushNotificationsDeliver3")) {
    res.writeHead(410, "GONE");
    res.end("");
    return;
  }

  else if (u.pathname === "/illegalhpacksoft") {
    // This will cause the compressor to compress a header that is not legal,
    // but only affects the stream, not the session.
    illegalheader_conn = req.stream.connection;
    Compressor.prototype.compress = insertSoftIllegalHpack;
    // Fall through to the default response behavior
  }

  else if (u.pathname === "/illegalhpackhard") {
    // This will cause the compressor to insert an HPACK instruction that will
    // cause a session failure.
    Compressor.prototype.compress = insertHardIllegalHpack;
    // Fall through to default response behavior
  }

  else if (u.pathname === "/illegalhpack_validate") {
    if (req.stream.connection === illegalheader_conn) {
      res.setHeader('X-Did-Goaway', 'no');
    } else {
      res.setHeader('X-Did-Goaway', 'yes');
    }
    // Fall through to the default response behavior
  }

  else if (u.pathname === "/foldedheader") {
    res.setHeader('X-Folded-Header', 'this is\n folded');
    // Fall through to the default response behavior
  }

  else if (u.pathname === "/emptydata") {
    // Overwrite the original transform with our version that will insert an
    // empty DATA frame at the beginning of the stream response, then fall
    // through to the default response behavior.
    Serializer.prototype._transform = newTransform;
  }

  // for use with test_immutable.js
  else if (u.pathname === "/immutable-test-without-attribute") {
     res.setHeader('Cache-Control', 'max-age=100000');
     res.setHeader('Etag', '1');
     if (req.headers["if-none-match"]) {
       res.setHeader("x-conditional", "true");
     }
    // default response from here
  }
  else if (u.pathname === "/immutable-test-with-attribute") {
    res.setHeader('Cache-Control', 'max-age=100000, immutable');
    res.setHeader('Etag', '2');
     if (req.headers["if-none-match"]) {
       res.setHeader("x-conditional", "true");
     }
   // default response from here
  }

  res.setHeader('Content-Type', 'text/html');
  if (req.httpVersionMajor != 2) {
    res.setHeader('Connection', 'close');
  }
  res.writeHead(200);
  res.end(content);
}

// Set up the SSL certs for our server - this server has a cert for foo.example.com
// signed by netwerk/tests/unit/CA.cert.der
//var log_module = node_http2_root + "/test/util";
var options = {
  key: fs.readFileSync(__dirname + '/http2-key.pem'),
  cert: fs.readFileSync(__dirname + '/http2-cert.pem'),
  //, log: require(log_module).createLogger('server')
};

var server = http2.createServer(options, handleRequest);

server.on('connection', function(socket) {
  socket.on('error', function() {
    // Ignoring SSL socket errors, since they usually represent a connection that was tore down
    // by the browser because of an untrusted certificate. And this happens at least once, when
    // the first test case if done.
  });
});

var serverPort;
function listenok() {
  serverPort = server._server.address().port;
  console.log('HTTP2 server listening on port ' + serverPort);
}
var portSelection = -1;
var envport = process.env.MOZHTTP2_PORT;
if (envport !== undefined) {
  try {
    portSelection = parseInt(envport, 10);
  } catch (e) {
    portSelection = -1;
  }
}
server.listen(portSelection, "0.0.0.0", 200, listenok);
