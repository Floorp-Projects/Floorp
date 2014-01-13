var fs = require('fs');
var path = require('path');
var http2 = require('..');

var options = process.env.HTTP2_PLAIN ? {
  plain: true
} : {
  key: fs.readFileSync(path.join(__dirname, '/localhost.key')),
  cert: fs.readFileSync(path.join(__dirname, '/localhost.crt'))
};

// Passing bunyan logger (optional)
options.log = require('../test/util').createLogger('server');

// We cache one file to be able to do simple performance tests without waiting for the disk
var cachedFile = fs.readFileSync(path.join(__dirname, './server.js'));
var cachedUrl = '/server.js';

// Creating the server
var server = http2.createServer(options, function(request, response) {
  var filename = path.join(__dirname, request.url);

  // Serving server.js from cache. Useful for microbenchmarks.
  if (request.url === cachedUrl) {
    response.end(cachedFile);
  }

  // Reading file from disk if it exists and is safe.
  else if ((filename.indexOf(__dirname) === 0) && fs.existsSync(filename) && fs.statSync(filename).isFile()) {
    response.writeHead('200');

    // If they download the certificate, push the private key too, they might need it.
    if (response.push && request.url === '/localhost.crt') {
      var push = response.push('/localhost.key');
      push.writeHead(200);
      fs.createReadStream(path.join(__dirname, '/localhost.key')).pipe(push);
    }

    fs.createReadStream(filename).pipe(response);
  }

  // Otherwise responding with 404.
  else {
    response.writeHead('404');
    response.end();
  }
});

server.listen(process.env.HTTP2_PORT || 8080);
