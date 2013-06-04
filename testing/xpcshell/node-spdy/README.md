# SPDY Server for node.js [![Build Status](https://secure.travis-ci.org/indutny/node-spdy.png)](http://travis-ci.org/indutny/node-spdy)

<a href="http://flattr.com/thing/758213/indutnynode-spdy-on-GitHub" target="_blank">
<img src="http://api.flattr.com/button/flattr-badge-large.png" alt="Flattr this" title="Flattr this" border="0" /></a>

With this module you can create [SPDY](http://www.chromium.org/spdy) servers
in node.js with natural http module interface and fallback to regular https
(for browsers that doesn't support SPDY yet).

## Usage

```javascript
var spdy = require('spdy'),
    fs = require('fs');

var options = {
  key: fs.readFileSync(__dirname + '/keys/spdy-key.pem'),
  cert: fs.readFileSync(__dirname + '/keys/spdy-cert.pem'),
  ca: fs.readFileSync(__dirname + '/keys/spdy-csr.pem'),

  // SPDY-specific options
  windowSize: 1024, // Server's window size
};

var server = spdy.createServer(options, function(req, res) {
  res.writeHead(200);
  res.end('hello world!');
});

server.listen(443);
```

And by popular demand - usage with
[express](https://github.com/visionmedia/express):

```javascript
var spdy = require('spdy'),
    express = require('express'),
    fs = require('fs');

var options = { /* the same as above */ };

var app = express();

app.use(/* your favorite middleware */);

var server = spdy.createServer(options, app);

server.listen(443);
```

## API

API is compatible with `http` and `https` module, but you can use another
function as base class for SPDYServer.

```javascript
spdy.createServer(
  [base class constructor, i.e. https.Server],
  { /* keys and options */ }, // <- the only one required argument
  [request listener]
).listen([port], [host], [callback]);
```

Request listener will receive two arguments: `request` and `response`. They're
both instances of `http`'s `IncomingMessage` and `OutgoingMessage`. But three
custom properties are added to both of them: `streamID`, `isSpdy`,
`spdyVersion`. The first one indicates on which spdy stream are sitting request
and response. Second is always true and can be checked to ensure that incoming
request wasn't received by HTTPS fallback and last one is a number representing
used SPDY protocol version (2 or 3 for now).

### Push streams

It is possible to initiate 'push' streams to send content to clients _before_
the client requests it.

```javascript
spdy.createServer(options, function(req, res) {
  var headers = { 'content-type': 'application/javascript' };
  res.push('/main.js', headers, function(err, stream) {
    if (err) return;

    stream.end('alert("hello from push stream!");');
  });

  res.end('<script src="/main.js"></script>');
}).listen(443);
```

Push is accomplished via the `push()` method invoked on the current response
object (this works for express.js response objects as well).  The format of the
`push()` method is:

`.push('full or relative url', { ... headers ... }, optional priority, callback)`

You can use either full ( `http://host/path` ) or relative ( `/path` ) urls with
`.push()`. `headers` are the same as for regular response object. `callback`
will receive two arguments: `err` (if any error is happened) and `stream`
(stream object have API compatible with a
[net.Socket](http://nodejs.org/docs/latest/api/net.html#net.Socket) ).

### Options

All options supported by
[tls](http://nodejs.org/docs/latest/api/tls.html#tls.createServer) are working
with node-spdy. In addition, `maxStreams` options is available. it allows you
controlling [maximum concurrent streams][http://www.chromium.org/spdy/spdy-protocol/spdy-protocol-draft2#TOC-SETTINGS]
protocol option (if client will start more streams than that limit, RST_STREAM
will be sent for each additional stream).

Additional options:

* `plain` - if defined, server will accept only plain (non-encrypted)
  connections.

#### Contributors

* [Fedor Indutny](https://github.com/indutny)
* [Chris Strom](https://github.com/eee-c)
* [François de Metz](https://github.com/francois2metz)
* [Ilya Grigorik](https://github.com/igrigorik)
* [Roberto Peon](https://github.com/grmocg)
* [Tatsuhiro Tsujikawa](https://github.com/tatsuhiro-t)
* [Jesse Cravens](https://github.com/jessecravens)

#### LICENSE

This software is licensed under the MIT License.

Copyright Fedor Indutny, 2012.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to permit
persons to whom the Software is furnished to do so, subject to the
following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.
