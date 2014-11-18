var assert = require('assert'),
    spdy = require('../../'),
    keys = require('../fixtures/keys'),
    http = require('http'),
    tls = require('tls'),
    Buffer = require('buffer').Buffer,
    PORT = 8081;

suite('A SPDY Server / Plain', function() {
  var server;
  setup(function(done) {
    server = spdy.createServer({ plain: true, ssl: false }, function(req, res) {
      res.end('ok');
    });

    server.listen(PORT, done);
  });

  teardown(function(done) {
    server.close(done);
  });

  test('should respond on regular http requests', function(done) {
    var req = http.request({
      host: '127.0.0.1',
      port: PORT,
      path: '/',
      method: 'GET',
      agent: false,
      rejectUnauthorized: false
    }, function(res) {
      res.on('data', function() {
        // Ignore incoming data
      });
      assert.equal(res.statusCode, 200);
      done();
    });
    req.end();
  });

  test('should respond on spdy requests', function(done) {
    var agent = spdy.createAgent({
      host: '127.0.0.1',
      port: PORT,
      spdy: {
        ssl: false,
        plain: true,
        version: 3
      }
    });

    var req = http.request({
      path: '/',
      method: 'GET',
      agent: agent,
    }, function(res) {
      res.on('data', function() {
        // Ignore incoming data
      });
      assert.equal(res.statusCode, 200);
      agent.close();
      done();
    });
    req.end();
  });

  test('should handle header values with colons', function(done) {
    var agent = spdy.createAgent({
      host: '127.0.0.1',
      port: PORT,
      spdy: {
        ssl: false,
        plain: true
      }
    });

    var refererValue = 'http://127.0.0.1:' + PORT + '/header-with-colon';

    server.on('request', function(req) {
      assert.equal(req.headers.referer, refererValue);
    });

    http.request({
      path: '/',
      method: 'GET',
      agent: agent,
      headers: { 'referer': refererValue }
    }, function(res) {
      assert.equal(res.statusCode, 200);
      agent.close();
      done();
    }).end();
  });

  test('should send date header as default', function(done) {
    var agent = spdy.createAgent({
      host: '127.0.0.1',
      port: PORT,
      spdy: {
        ssl: false,
        plain: true
      }
    });

    http.request({
      path: '/',
      method: 'GET',
      agent: agent
    }, function(res) {
      assert.equal(typeof res.headers.date, 'string');
      agent.close();
      done();
    }).end();
  });

  test('should not send date header if res.sendDate is false', function(done) {
    var agent = spdy.createAgent({
      host: '127.0.0.1',
      port: PORT,
      spdy: {
        ssl: false,
        plain: true
      }
    });

    server.removeAllListeners('request');
    server.on('request', function(req, res) {
      res.sendDate = false;
      res.end('ok');
    });

    http.request({
      path: '/',
      method: 'GET',
      agent: agent
    }, function(res) {
      assert.equal(typeof res.headers.date, 'undefined');
      agent.close();
      done();
    }).end();
  });
});
