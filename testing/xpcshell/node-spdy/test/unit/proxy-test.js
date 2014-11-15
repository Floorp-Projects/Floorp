var assert = require('assert'),
    https = require('https'),
    spdy = require('../../'),
    keys = require('../fixtures/keys'),
    net = require('net'),
    url = require('url'),
    PORT = 8081;

suite('A SPDY server / Proxy', function() {
  test('should emit connect event on CONNECT requests', function(done) {
    var proxyServer = spdy.createServer(keys);
    proxyServer.on('connect', function(req, socket) {
      var srvUrl = url.parse('http://' + req.url);
      var srvSocket = net.connect(srvUrl.port, srvUrl.hostname, function() {
        socket._lock(function() {
          var headers = {
            'Connection': 'keep-alive',
            'Proxy-Agent': 'SPDY Proxy'
          }
          socket._spdyState.framer.replyFrame(
            socket._spdyState.id, 200, "Connection Established", headers,
            function (err, frame) {
              socket.connection.write(frame);
              socket._unlock();
              srvSocket.pipe(socket);
              socket.pipe(srvSocket);
            }
          );
        });
      });
    });

    proxyServer.listen(PORT, '127.0.0.1', function() {
      var spdyAgent = spdy.createAgent({
        host: '127.0.0.1',
        port: PORT,
        rejectUnauthorized: false
      });

      var options = {
        method: 'CONNECT',
        path: 'www.google.com:80',
        agent: spdyAgent
      };

      var req = https.request(options);
      req.end();

      req.on('connect', function(res, socket) {
        var googlePage = "";
        socket.write('GET / HTTP/1.1\r\n' +
                     'Host: www.google.com:80\r\n' +
                     'Connection: close\r\n' +
                     '\r\n');

        socket.on('data', function(chunk) {
          googlePage = googlePage + chunk.toString();
        });

        socket.on('end', function() {
          assert.notEqual(googlePage.search('google'), -1,
            "Google page should contain string 'google'");
          spdyAgent.close(function() {
            proxyServer.close(done);
          });
        });
      });
    });
  });
});
