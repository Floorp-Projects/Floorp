/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var http2 = require('../node-http2');
var fs = require('fs');
var url = require('url');
var crypto = require('crypto');

// Hook into the decompression code to log the decompressed name-value pairs
var http2_compression = require('../node-http2/node_modules/http2-protocol/lib/compressor');
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

function handleRequest(req, res) {
  var u = url.parse(req.url);
  var content = getHttpContent(u.pathname);
  var push;

  if (req.httpVersionMajor === 2) {
    res.setHeader('X-Connection-Http2', 'yes');
    res.setHeader('X-Http2-StreamId', '' + req.stream.id);
  } else {
    res.setHeader('X-Connection-Http2', 'no');
  }

  if (u.pathname === '/exit') {
    res.setHeader('Content-Type', 'text/plain');
    res.writeHead(200);
    res.end('ok');
    process.exit();
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

  else if (u.pathname === "/big") {
    content = generateContent(128 * 1024);
    var hash = crypto.createHash('md5');
    hash.update(content);
    var md5 = hash.digest('hex');
    res.setHeader("X-Expected-MD5", md5);
  }

  else if (u.pathname === "/post") {
    if (req.method != "POST") {
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

  res.setHeader('Content-Type', 'text/html');
  res.writeHead(200);
  res.end(content);
}

// Set up the SSL certs for our server
var options = {
  key: fs.readFileSync(__dirname + '/../moz-spdy/spdy-key.pem'),
  cert: fs.readFileSync(__dirname + '/../moz-spdy/spdy-cert.pem'),
  ca: fs.readFileSync(__dirname + '/../moz-spdy/spdy-ca.pem'),
  //, log: require('../node-http2/test/util').createLogger('server')
};

var server = http2.createServer(options, handleRequest);
server.on('connection', function(socket) {
  socket.on('error', function() {
    // Ignoring SSL socket errors, since they usually represent a connection that was tore down
    // by the browser because of an untrusted certificate. And this happens at least once, when
    // the first test case if done.
  });
});
server.listen(6944);
console.log('HTTP2 server listening on port 6944');
