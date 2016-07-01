var assert = require('assert'),
    spdy = require('../../'),
    zlib = require('zlib'),
    keys = require('../fixtures/keys'),
    https = require('https'),
    tls = require('tls'),
    Buffer = require('buffer').Buffer,
    PORT = 8081;

suite('A SPDY Server / Connect', function() {
  var server;
  var fox = 'The quick brown fox jumps over the lazy dog';

  setup(function(done) {
    server = spdy.createServer(keys, function(req, res) {
      var comp = req.url === '/gzip' ? zlib.createGzip() :
                 req.url === '/deflate' ? zlib.createDeflate() :
                 null;

      if (req.url == '/headerTest') {
        if (req.headers['accept-encoding'] == 'gzip, deflate')
          return res.end(fox);
        else
          return res.end();
      }

      // Terminate connection gracefully
      if (req.url === '/goaway')
        req.socket.connection.end();

      if (!comp)
        return res.end(fox);

      res.writeHead(200, { 'Content-Encoding' : req.url.slice(1) });
      comp.on('data', function(chunk) {
        res.write(chunk);
      });
      comp.once('end', function() {
        res.end();
      });
      comp.end(fox);
    });

    server.listen(PORT, done);
  });

  teardown(function(done) {
    server.close(done);
  });

  test('should respond on regular https requests', function(done) {
    var req = https.request({
      host: '127.0.0.1',
      port: PORT,
      path: '/',
      method: 'GET',
      agent: false,
      rejectUnauthorized: false
    }, function(res) {
      var received = '';
      res.on('data', function(chunk) {
        received += chunk;
      });
      res.once('end', function() {
        assert.equal(received, fox);
        done();
      });
      assert.equal(res.statusCode, 200);
    });
    req.end();
  });

  function spdyReqTest(url) {
    test('should respond on spdy requests on ' + url, function(done) {
      var agent = spdy.createAgent({
        host: '127.0.0.1',
        port: PORT,
        rejectUnauthorized: false
      });

      var req = https.request({
        path: url,
        method: 'GET',
        agent: agent,
      }, function(res) {
        var received = '';
        res.on('data', function(chunk) {
          received += chunk;
        });
        res.once('end', function() {
          assert.equal(received, fox);
          agent.close();
          done();
        });
        assert.equal(res.statusCode, 200);
      });
      req.end();
    });
  }

  spdyReqTest('/');
  spdyReqTest('/gzip');
  spdyReqTest('/deflate');

  test('should not fail at a lot of RSTs', function(done) {
    var s = tls.connect({
      host: '127.0.0.1',
      port: PORT,
      NPNProtocols: [ 'spdy/3' ],
      rejectUnauthorized: false
    }, function() {
      var rst = new Buffer([
        0x80, 0x03, 0x00, 0x03,
        0x00, 0x00, 0x00, 0x08,
        0x00, 0x00, 0x00, 0x02,
        0x00, 0x00, 0x00, 0x00
      ]);
      var rsts = [];
      for (var i = 0; i < 1000; i++)
        rsts.push(rst);
      rsts = Buffer.concat(rsts, rst.length * rsts.length);
      for (var i = 0; i < 10; i++)
        s.write(rsts);
      s.write(rsts, function() {
        s.destroy();
        done();
      });
    });
  });

  test('should not decompress stream when decompress is false', function(done) {
      var agent = spdy.createAgent({
        host: '127.0.0.1',
        port: PORT,
        rejectUnauthorized: false,
        spdy: {
          decompress: false
        }
      });

      var req = https.request({
        path: '/gzip',
        method: 'GET',
        agent: agent,
      }, function(res) {
        var received = [];
        res.on('data', function(chunk) {
          received.push(chunk);
        });
        res.once('end', function() {
          agent.close();

          zlib.gunzip(Buffer.concat(received), function(err, decompressed) {
            assert.equal(decompressed, fox);
            done();
          });
        });
        assert.equal(res.statusCode, 200);
        assert.equal(res.headers['content-encoding'], 'gzip');
      });
      req.end();
  });

  test('should not create RangeErrors on client errors', function(done) {
    // https://github.com/indutny/node-spdy/issues/147
    var agent = spdy.createAgent({
      host: '127.0.0.1',
      port: PORT,
      rejectUnauthorized: true
    }).on('error', function (err) {
      assert(err.message === 'self signed certificate' ||
        err.message === 'DEPTH_ZERO_SELF_SIGNED_CERT');
    });

    var req = https.request({
      path: '/',
      method: 'GET',
      agent: agent
    });
    req.on('error', function (err) {
      assert.equal(err.code, 'ECONNRESET');
      done();
    });
    req.end();
  });

  test('should not support GOAWAY', function(done) {
    // https://github.com/indutny/node-spdy/issues/147
    var agent = spdy.createAgent({
      host: '127.0.0.1',
      port: PORT,
      rejectUnauthorized: false
    });

    var total = '';
    var req = https.request({
      path: '/goaway',
      method: 'GET',
      agent: agent
    }, function(res) {
      res.on('data', function(chunk) {
        total += chunk;
      });
    });
    req.end();

    agent._spdyState.socket.once('close', function() {
      assert.equal(total, fox);
      done();
    });
  });

  test('should add accept-encoding header to request headers, ' +
           'if none present',
       function(done) {
    var agent = spdy.createAgent({
      host: '127.0.0.1',
      port: PORT,
      rejectUnauthorized: false
    });

    var req = https.request({
        path: '/headerTest',
        method: 'GET',
        agent: agent,
      }, function(res) {
        var total = '';
        res.on('data', function(chunk){
          total += chunk;
        });
        res.once('end', function() {
          agent.close();
          assert.equal(total, fox);
          done();
        });
      });
      req.end();
  });
});
