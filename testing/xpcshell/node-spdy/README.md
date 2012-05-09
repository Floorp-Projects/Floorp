# SPDY Server for node.js [![Build Status](https://secure.travis-ci.org/indutny/node-spdy.png)](http://travis-ci.org/indutny/node-spdy)

With this module you can create [SPDY](http://www.chromium.org/spdy) servers
in node.js with natural http module interface and fallback to regular https
(for browsers that doesn't support SPDY yet).

## Node+OpenSSL building

At the moment node-spdy requires zlib dictionary support, which will come to
node.js only in 0.7.x version. To build 0.7.x version follow instructions below:

```bash
git clone git://github.com/joyent/node.git
cd node
./configure --prefix=$HOME/.node/dev # <- or any other dir

make install -j4 # in -jN, N is number of CPU cores on your machine

# Add node's bin to PATH env variable
echo 'export PATH=$HOME/.node/dev/bin:$PATH' >> ~/.bashrc

#
# You have working node 0.7.x + NPN now !!!
#
```

## Usage

```javascript
var spdy = require('spdy');

var options = {
  key: fs.readFileSync(__dirname + '/keys/spdy-key.pem'),
  cert: fs.readFileSync(__dirname + '/keys/spdy-cert.pem'),
  ca: fs.readFileSync(__dirname + '/keys/spdy-csr.pem')
};

spdy.createServer(options, function(req, res) {
  res.writeHead(200);
  res.end('hello world!');
});

spdy.listen(443);
```

## API

API is compatible with `http` and `https` module, but you can use another
function as base class for SPDYServer. For example,
`require('express').HTTPSServer` given that as base class you'll get a server
compatible with [express](https://github.com/visionmedia/express) API.

```javascript
spdy.createServer(
  [base class constructor, i.e. https.Server or express.HTTPSServer],
  { /* keys and options */ }, // <- the only one required argument
  [request listener]
).listen([port], [host], [callback]);
```

Request listener will receive two arguments: `request` and `response`. They're
both instances of `http`'s `IncomingMessage` and `OutgoingMessage`. But two
custom properties are added to both of them: `streamID` and `isSpdy`. The first
one indicates on which spdy stream are sitting request and response. Latter one
is always true and can be checked to ensure that incoming request wasn't
received by HTTPS callback.

### Push streams

It's possible to initiate 'push' stream to send content to client, before one'll
request it.

```javascript
spdy.createServer(options, function(req, res) {
  var headers = { 'content-type': 'application/javascript' };
  res.send('/main.js', headers, function(err, stream) {
    if (err) return;

    stream.end('alert("hello from push stream!");');
  });

  res.end('<script src="/main.js"></script>');
}).listen(443);
```

`.push('full or relative url', { ... headers ... }, callback)`

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

#### Contributors

* [Fedor Indutny](https://github.com/indutny)
* [Chris Storm](https://github.com/eee-c)
* [François de Metz](https://github.com/francois2metz)

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
