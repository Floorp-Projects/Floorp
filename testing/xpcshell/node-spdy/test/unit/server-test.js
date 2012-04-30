var assert = require('assert'),
    spdy = require('../../'),
    keys = require('../fixtures/keys'),
    https = require('https'),
    tls = require('tls'),request
    Buffer = require('buffer').Buffer;


suite('A SPDY Server', function() {
  var server;
  setup(function(done) {
    server = spdy.createServer(keys, function(req, res) {
      res.end('ok');
    });

    server.listen(8081, done);
  });

  teardown(function(done) {
    server.once('close', done);
    server.close();
  });

  test('should respond on regular https requests', function(done) {
    https.request({
      host: 'localhost',
      port: 8081,
      path: '/',
      method: 'GET'
    }, function(res) {
      assert.equal(res.statusCode, 200);
      done();
    }).end();
  });

  test('should respond on spdy requests', function(done) {
    var socket = tls.connect(
      8081,
      'localhost',
      { NPNProtocols: ['spdy/2'] },
      function() {
        var deflate = spdy.utils.createDeflate(),
            chunks = [],
            length = 0;

        deflate.on('data', function(chunk) {
          chunks.push(chunk);
          length += chunk.length;
        });

        // Deflate headers
        deflate.write(new Buffer([
          0x00, 0x04, // method, url, version = 3 fields
          0x00, 0x06,
          0x6d, 0x65, 0x74, 0x68, 0x6f, 0x64, // method
          0x00, 0x03,
          0x47, 0x45, 0x54, // get
          0x00, 0x03,
          0x75, 0x72, 0x6c, // url
          0x00, 0x01,
          0x2f, // '/'
          0x00, 0x07,
          0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, // version
          0x00, 0x08,
          0x48, 0x54, 0x54, 0x50, 0x2f, 0x31, 0x2e, 0x31, // HTTP/1.1
          0x00, 0x04,
          0x68, 0x6f, 0x73, 0x74, // host
          0x00, 0x09,
          0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x68, 0x6f, 0x73, 0x74 //localhost
        ]));

        deflate.flush(function() {
          // StreamID + Associated StreamID
          length += 10;

          // Headers
          socket.write(new Buffer([
            0x80, 0x02, 0x00, 0x01, // Control, Version, SYN_STREAM
            0x00, 0x00, 0x00, length, // Flags, length (1 byte in this case)
            0x00, 0x00, 0x00, 0x01, // StreamID
            0x00, 0x00, 0x00, 0x00, // Associated StreamID
            0x00, 0x00 // Priority + Unused
          ]));

          // Write compressed headers
          chunks.forEach(function(chunk) {
            socket.write(chunk);
          });
        });

        var response = new Buffer(85),
            offset = 0;

        socket.on('data', function(chunk) {
          assert.ok(offset + chunk.length <= 85);

          chunk.copy(response, offset);
          offset += chunk.length;

          if (offset === 85) {
            var frames = [];

            offset = 0;
            while (offset < response.length) {
              var len = (response.readUInt32BE(offset + 4) & 0x00ffffff) + 8;
              frames.push(response.slice(offset, offset + len));

              offset += len;
            }

            // SYN_STREAM frame
            assert.ok(frames.some(function(frame) {
              return frame[0] === 0x80 && // Control frame
                     frame[1] === 0x02 && // Version
                     frame.readUInt16BE(2) === 0x0002 && // SYN_STREAM
                     frame.readUInt32BE(8) === 0x0001; // StreamID
            }));

            // Data frames
            assert.ok(frames.some(function(frame) {
              return frame[0] === 0x00 && // Data frame
                     frame.readUInt32BE(0) === 0x0001 && // StreamID
                     frame.slice(8).toString() === 'ok';
            }));

            socket.destroy();
          }
        });

        socket.on('close', function() {
          done();
        });
      }
    );

    server.on('request', function(req, res) {
      assert.equal(req.url, '/');
      assert.equal(req.method, 'GET');
      res.end('ok');
    });
  });
});
