var fs = require('fs');
var path = require('path');
var parse_url = require('url').parse;
var http2 = require('..');
var tls = require('tls');

// Advertised protocol version
var implementedVersion = http2.ImplementedVersion;

// Bunyan logger
var log = require('../test/util').createLogger('client');

// Parsing the URL
var url = parse_url(process.argv.pop())

// Connecting to the server
var socket = tls.connect(url.port, url.hostname, {
  rejectUnauthorized: false,
  ALPNProtocols: [implementedVersion],
  NPNProtocols: [implementedVersion],
  servername: url.hostname
}, onConnection);

// Handling the connection
function onConnection() {
  var endpoint = new http2.Endpoint(log, 'CLIENT', {});
  endpoint.pipe(socket).pipe(endpoint);

  // Sending request
  var stream = endpoint.createStream();
  stream.headers({
    ':method': 'GET',
    ':scheme': url.protocol.slice(0, url.protocol.length - 1),
    ':authority': url.hostname,
    ':path': url.path + (url.hash || '')
  });

  // Receiving push streams
  stream.on('promise', function(push_stream, req_headers) {
    var filename = path.join(__dirname, '/push-' + push_count);
    push_count += 1;
    console.error('Receiving pushed resource: ' + req_headers[':path'] + ' -> ' + filename);
    push_stream.pipe(fs.createWriteStream(filename)).on('finish', finish);
  });

  // Receiving the response body
  stream.pipe(process.stdout);
  stream.on('end', finish);
}

// Quitting after both the response and the associated pushed resources have arrived
var push_count = 0;
var finished = 0;
function finish() {
  finished += 1;
  if (finished === (1 + push_count)) {
    process.exit();
  }
}
