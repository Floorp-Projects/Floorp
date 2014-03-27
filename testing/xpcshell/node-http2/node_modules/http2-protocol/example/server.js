var fs = require('fs');
var path = require('path');
var http2 = require('..');
var tls = require('tls');

var exists = fs.existsSync;
var stat = fs.statSync;
var read = fs.createReadStream;
var join = path.join;

// Advertised protocol version
var implementedVersion = http2.ImplementedVersion;

// Bunyan logger
var log = require('../test/util').createLogger('server');

// We cache one file to be able to do simple performance tests without waiting for the disk
var cachedFile = fs.readFileSync(path.join(__dirname, './server.js'));
var cachedUrl = '/server.js';

// Creating the server
tls.createServer({
  ALPNProtocols: [implementedVersion],
  NPNProtocols: [implementedVersion],
  key: fs.readFileSync(path.join(__dirname, '/localhost.key')),
  cert: fs.readFileSync(path.join(__dirname, '/localhost.crt'))
}).on('secureConnection', onConnection).listen(process.env.HTTP2_PORT || 8080);

// Handling incoming connections
function onConnection(socket) {
  var endpoint = new http2.Endpoint(log, 'SERVER', {});
  endpoint.pipe(socket).pipe(endpoint);

  endpoint.on('stream', function(stream) {
    stream.on('headers', function(headers) {
      var path = headers[':path'];
      var filename = join(__dirname, path);

      // Serving server.js from cache. Useful for microbenchmarks.
      if (path == cachedUrl) {
        stream.headers({ ':status': 200 });
        stream.end(cachedFile);
      }

      // Reading file from disk if it exists and is safe.
      else if ((filename.indexOf(__dirname) === 0) && exists(filename) && stat(filename).isFile()) {
        stream.headers({ ':status': 200 });

        // If they download the certificate, push the private key too, they might need it.
        if (path === '/localhost.crt') {
          var push = stream.promise({
            ':method': 'GET',
            ':scheme': headers[':scheme'],
            ':authority': headers[':authority'],
            ':path': '/localhost.key'
          });
          push.headers({ ':status': 200 });
          read(join(__dirname, '/localhost.key')).pipe(push);
        }

        read(filename).pipe(stream);
      }

      // Otherwise responding with 404.
      else {
        stream.headers({ ':status': 404 });
        stream.end();
      }
    });
  });
}
