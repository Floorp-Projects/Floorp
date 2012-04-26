var spdy = require('../../lib/spdy'),
    Buffer = require('buffer').Buffer;

exports.createSynStream = function(host, url, callback) {
  var deflate = spdy.utils.createDeflate(),
      chunks = [],
      chunksTotal = 0,
      syn_stream;

  deflate.on('data', function(chunk) {
    chunks.push(chunk);
    chunksTotal += chunk.length;
  });
  deflate.write(new Buffer([ 0x00, 0x02, 0x00, 0x04 ]));
  deflate.write('host');
  deflate.write(new Buffer([ 0x00, host.length ]));
  deflate.write(host);
  deflate.write(new Buffer([ 0x00, 0x03 ]));
  deflate.write('url');
  deflate.write(new Buffer([ 0x00, url.length ]));
  deflate.write(url);

  deflate.flush(function() {
    syn_stream = new Buffer(18 + chunksTotal);
    syn_stream.writeUInt32BE(0x80020001, 0);
    syn_stream.writeUInt32BE(chunksTotal + 8, 4);
    syn_stream.writeUInt32BE(0x00000001, 8);
    syn_stream.writeUInt32BE(0x00000000, 12);

    var offset = 18;
    chunks.forEach(function(chunk) {
      chunk.copy(syn_stream, offset);
      offset += chunk.length;
    });

    callback(syn_stream);
  });
};
