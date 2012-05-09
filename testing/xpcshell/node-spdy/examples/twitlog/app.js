/**
 * Module dependencies.
 */

var fs = require('fs'),
    io = require('socket.io'),
    express = require('express'),
    spdy = require('../../');
    routes = require('./routes');
    realtime = require('./realtime');

var options = {
  key: fs.readFileSync(__dirname + '/keys/twitlog-key.pem'),
  cert: fs.readFileSync(__dirname + '/keys/twitlog-cert.pem'),
  ca: fs.readFileSync(__dirname + '/keys/twitlog-csr.pem'),
  ciphers: '!aNULL:!ADH:!eNull:!LOW:!EXP:RC4+RSA:MEDIUM:HIGH',
  maxStreams: 15
};

var app = module.exports = spdy.createServer(express.HTTPSServer, options);

io = io.listen(app, { log: false });
io.set('transports', ['xhr-polling', 'jsonp-polling']);

// Configuration

app.configure(function(){
  app.set('views', __dirname + '/views');
  app.set('view engine', 'jade');
  app.use(express.bodyParser());
  app.use(express.methodOverride());
  app.use(app.router);
  app.use(express.staticCache());
  app.use(express.static(__dirname + '/public'));
});

app.configure('development', function(){
  app.use(express.errorHandler({ dumpExceptions: true, showStack: true }));
});

app.configure('production', function(){
  app.use(express.errorHandler());
});

// Routes

app.get('/', routes.index);

// Socket.io

realtime.init(io);

app.listen(8081);
console.log(
  'Express server listening on port %d in %s mode',
  app.address().port,
  app.settings.env
);

// Do not fail on exceptions

process.on('uncaughtException', function(err) {
  console.error(err.stack);
});
