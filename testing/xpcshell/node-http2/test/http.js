var expect = require('chai').expect;
var util = require('./util');
var fs = require('fs');
var path = require('path');

var http2 = require('../lib/http');
var https = require('https');

process.env.NODE_TLS_REJECT_UNAUTHORIZED = "0";

var options = {
  key: fs.readFileSync(path.join(__dirname, '../example/localhost.key')),
  cert: fs.readFileSync(path.join(__dirname, '../example/localhost.crt')),
  log: util.log
};

http2.globalAgent = new http2.Agent({ log: util.log });

describe('http.js', function() {
  describe('Server', function() {
    describe('new Server(options)', function() {
      it('should throw if called without \'plain\' or TLS options', function() {
        expect(function() {
          new http2.Server();
        }).to.throw(Error);
        expect(function() {
          http2.createServer(util.noop);
        }).to.throw(Error);
      });
    });
    describe('property `timeout`', function() {
      it('should be a proxy for the backing HTTPS server\'s `timeout` property', function() {
        var server = new http2.Server(options);
        var backingServer = server._server;
        var newTimeout = 10;
        server.timeout = newTimeout;
        expect(server.timeout).to.be.equal(newTimeout);
        expect(backingServer.timeout).to.be.equal(newTimeout);
      });
    });
    describe('method `setTimeout(timeout, [callback])`', function() {
      it('should be a proxy for the backing HTTPS server\'s `setTimeout` method', function() {
        var server = new http2.Server(options);
        var backingServer = server._server;
        var newTimeout = 10;
        var newCallback = util.noop;
        backingServer.setTimeout = function(timeout, callback) {
          expect(timeout).to.be.equal(newTimeout);
          expect(callback).to.be.equal(newCallback);
        };
        server.setTimeout(newTimeout, newCallback);
      });
    });
  });
  describe('Agent', function() {
    describe('property `maxSockets`', function() {
      it('should be a proxy for the backing HTTPS agent\'s `maxSockets` property', function() {
        var agent = new http2.Agent({ log: util.log });
        var backingAgent = agent._httpsAgent;
        var newMaxSockets = backingAgent.maxSockets + 1;
        agent.maxSockets = newMaxSockets;
        expect(agent.maxSockets).to.be.equal(newMaxSockets);
        expect(backingAgent.maxSockets).to.be.equal(newMaxSockets);
      });
    });
    describe('method `request(options, [callback])`', function() {
      it('should throw when trying to use with \'http\' scheme', function() {
        expect(function() {
          var agent = new http2.Agent({ log: util.log });
          agent.request({ protocol: 'http:' });
        }).to.throw(Error);
      });
    });
  });
  describe('OutgoingRequest', function() {
    function testFallbackProxyMethod(name, originalArguments, done) {
      var request = new http2.OutgoingRequest();

      // When in HTTP/2 mode, this call should be ignored
      request.stream = { reset: util.noop };
      request[name].apply(request, originalArguments);
      delete request.stream;

      // When in fallback mode, this call should be forwarded
      request[name].apply(request, originalArguments);
      var mockFallbackRequest = { on: util.noop };
      mockFallbackRequest[name] = function() {
        expect(arguments).to.deep.equal(originalArguments);
        done();
      };
      request._fallback(mockFallbackRequest);
    }
    describe('method `setNoDelay(noDelay)`', function() {
      it('should act as a proxy for the backing HTTPS agent\'s `setNoDelay` method', function(done) {
        testFallbackProxyMethod('setNoDelay', [true], done);
      });
    });
    describe('method `setSocketKeepAlive(enable, initialDelay)`', function() {
      it('should act as a proxy for the backing HTTPS agent\'s `setSocketKeepAlive` method', function(done) {
        testFallbackProxyMethod('setSocketKeepAlive', [true, util.random(10, 100)], done);
      });
    });
    describe('method `setTimeout(timeout, [callback])`', function() {
      it('should act as a proxy for the backing HTTPS agent\'s `setTimeout` method', function(done) {
        testFallbackProxyMethod('setTimeout', [util.random(10, 100), util.noop], done);
      });
    });
    describe('method `abort()`', function() {
      it('should act as a proxy for the backing HTTPS agent\'s `abort` method', function(done) {
        testFallbackProxyMethod('abort', [], done);
      });
    });
  });
  describe('test scenario', function() {
    describe('simple request', function() {
      it('should work as expected', function(done) {
        var path = '/x';
        var message = 'Hello world';

        var server = http2.createServer(options, function(request, response) {
          expect(request.url).to.equal(path);
          response.end(message);
        });

        server.listen(1234, function() {
          http2.get('https://localhost:1234' + path, function(response) {
            response.on('data', function(data) {
              expect(data.toString()).to.equal(message);
              server.close();
              done();
            });
          });
        });
      });
    });
    describe('request with payload', function() {
      it('should work as expected', function(done) {
        var path = '/x';
        var message = 'Hello world';

        var server = http2.createServer(options, function(request, response) {
          expect(request.url).to.equal(path);
          request.once('data', function(data) {
            expect(data.toString()).to.equal(message);
            response.end();
          });
        });

        server.listen(1240, function() {
          var request = http2.request({
            host: 'localhost',
            port: 1240,
            path: path
          });
          request.write(message);
          request.end();
          request.on('response', function() {
            server.close();
            done();
          });
        });
      });
    });
    describe('request with custom status code and headers', function() {
      it('should work as expected', function(done) {
        var path = '/x';
        var message = 'Hello world';
        var headerName = 'name';
        var headerValue = 'value';

        var server = http2.createServer(options, function(request, response) {
          // Request URL and headers
          expect(request.url).to.equal(path);
          expect(request.headers[headerName]).to.equal(headerValue);

          // A header to be overwritten later
          response.setHeader(headerName, 'to be overwritten');
          expect(response.getHeader(headerName)).to.equal('to be overwritten');

          // A header to be deleted
          response.setHeader('nonexistent', 'x');
          response.removeHeader('nonexistent');
          expect(response.getHeader('nonexistent')).to.equal(undefined);

          // Don't send date
          response.sendDate = false;

          // Specifying more headers, the status code and a reason phrase with `writeHead`
          var moreHeaders = {};
          moreHeaders[headerName] = headerValue;
          response.writeHead(600, 'to be discarded', moreHeaders);
          expect(response.getHeader(headerName)).to.equal(headerValue);

          // Empty response body
          response.end(message);
        });

        server.listen(1239, function() {
          var headers = {};
          headers[headerName] = headerValue;
          var request = http2.request({
            host: 'localhost',
            port: 1239,
            path: path,
            headers: headers
          });
          request.end();
          request.on('response', function(response) {
            expect(response.headers[headerName]).to.equal(headerValue);
            expect(response.headers['nonexistent']).to.equal(undefined);
            expect(response.headers['date']).to.equal(undefined);
            response.on('data', function(data) {
              expect(data.toString()).to.equal(message);
              server.close();
              done();
            });
          });
        });
      });
    });
    describe('request over plain TCP', function() {
      it('should work as expected', function(done) {
        var path = '/x';
        var message = 'Hello world';

        var server = http2.createServer({
          plain: true,
          log: util.log
        }, function(request, response) {
          expect(request.url).to.equal(path);
          response.end(message);
        });

        server.listen(1237, function() {
          var request = http2.request({
            plain: true,
            host: 'localhost',
            port: 1237,
            path: path
          }, function(response) {
            response.on('data', function(data) {
              expect(data.toString()).to.equal(message);
              server.close();
              done();
            });
          });
          request.end();
        });
      });
    });
    describe('request to an HTTPS/1 server', function() {
      it('should fall back to HTTPS/1 successfully', function(done) {
        var path = '/x';
        var message = 'Hello world';

        var server = https.createServer(options, function(request, response) {
          expect(request.url).to.equal(path);
          response.end(message);
        });

        server.listen(5678, function() {
          http2.get('https://localhost:5678' + path, function(response) {
            response.on('data', function(data) {
              expect(data.toString()).to.equal(message);
              done();
            });
          });
        });
      });
    });
    describe('HTTPS/1 request to a HTTP/2 server', function() {
      it('should fall back to HTTPS/1 successfully', function(done) {
        var path = '/x';
        var message = 'Hello world';

        var server = http2.createServer(options, function(request, response) {
          expect(request.url).to.equal(path);
          response.end(message);
        });

        server.listen(1236, function() {
          https.get('https://localhost:1236' + path, function(response) {
            response.on('data', function(data) {
              expect(data.toString()).to.equal(message);
              done();
            });
          });
        });
      });
    });
    describe('two parallel request', function() {
      it('should work as expected', function(done) {
        var path = '/x';
        var message = 'Hello world';

        var server = http2.createServer(options, function(request, response) {
          expect(request.url).to.equal(path);
          response.end(message);
        });

        server.listen(1237, function() {
          done = util.callNTimes(2, done);
          // 1. request
          http2.get('https://localhost:1237' + path, function(response) {
            response.on('data', function(data) {
              expect(data.toString()).to.equal(message);
              done();
            });
          });
          // 2. request
          http2.get('https://localhost:1237' + path, function(response) {
            response.on('data', function(data) {
              expect(data.toString()).to.equal(message);
              done();
            });
          });
        });
      });
    });
    describe('two subsequent request', function() {
      it('should use the same HTTP/2 connection', function(done) {
        var path = '/x';
        var message = 'Hello world';

        var server = http2.createServer(options, function(request, response) {
          expect(request.url).to.equal(path);
          response.end(message);
        });

        server.listen(1238, function() {
          // 1. request
          http2.get('https://localhost:1238' + path, function(response) {
            response.on('data', function(data) {
              expect(data.toString()).to.equal(message);

              // 2. request
              http2.get('https://localhost:1238' + path, function(response) {
                response.on('data', function(data) {
                  expect(data.toString()).to.equal(message);
                  done();
                });
              });
            });
          });
        });
      });
    });
    describe('request and response with trailers', function() {
      it('should work as expected', function(done) {
        var path = '/x';
        var message = 'Hello world';
        var requestTrailers = { 'content-md5': 'x' };
        var responseTrailers = { 'content-md5': 'y' };

        var server = http2.createServer(options, function(request, response) {
          expect(request.url).to.equal(path);
          request.on('data', util.noop);
          request.once('end', function() {
            expect(request.trailers).to.deep.equal(requestTrailers);
            response.write(message);
            response.addTrailers(responseTrailers);
            response.end();
          });
        });

        server.listen(1241, function() {
          var request = http2.request('https://localhost:1241' + path);
          request.addTrailers(requestTrailers);
          request.end();
          request.on('response', function(response) {
            response.on('data', util.noop);
            response.once('end', function() {
              expect(response.trailers).to.deep.equal(responseTrailers);
              done();
            });
          });
        });
      });
    });
    describe('server push', function() {
      it('should work as expected', function(done) {
        var path = '/x';
        var message = 'Hello world';
        var pushedPath = '/y';
        var pushedMessage = 'Hello world 2';

        var server = http2.createServer(options, function(request, response) {
          expect(request.url).to.equal(path);
          var push1 = response.push('/y');
          push1.end(pushedMessage);
          var push2 = response.push({ path: '/y', protocol: 'https:' });
          push2.end(pushedMessage);
          response.end(message);
        });

        server.listen(1235, function() {
          var request = http2.get('https://localhost:1235' + path);
          done = util.callNTimes(5, done);

          request.on('response', function(response) {
            response.on('data', function(data) {
              expect(data.toString()).to.equal(message);
              done();
            });
            response.on('end', done);
          });

          request.on('push', function(promise) {
            expect(promise.url).to.be.equal(pushedPath);
            promise.on('response', function(pushStream) {
              pushStream.on('data', function(data) {
                expect(data.toString()).to.equal(pushedMessage);
                done();
              });
              pushStream.on('end', done);
            });
          });
        });
      });
    });
  });
});
