var tls = require('tls'),
    frames = require('../fixtures/frames');

var uri = require('url').parse(process.argv[2]),
    host = uri.hostname,
    port = +uri.port,
    url = uri.path;

frames.createSynStream(host, url, function(syn_stream) {
  var start = +new Date,
      num = 3000;

  batch(port, host, syn_stream, 100, num, function() {
    var end = +new Date;
    console.log('requests/sec : %d', 1000 * num / (end - start));
  });
});

function request(port, host, data, callback) {
  var socket = tls.connect(port, host, {NPNProtocols: ['spdy/2']}, function() {
    socket.write(data);
    socket.once('data', function() {
      socket.destroy();
      callback();
    });
  });

  return socket;
};

function batch(port, host, data, parallel, num, callback) {
  var left = num,
      done = 0;

  for (var i = 0; i < parallel; i++) {
    run(i);
  }

  function run(i) {
    left--;
    if (left < 0) return;

    request(port, host, data, function() {
      if (++done ===  num) return callback();
      run(i);
    });
  }
};
