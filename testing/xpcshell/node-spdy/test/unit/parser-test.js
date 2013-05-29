var assert = require('assert'),
    spdy = require('../../'),
    Buffer = require('buffer').Buffer;

suite('A Parser of SPDY module', function() {
  var parser;

  [2,3].forEach(function(version) {
    suite('version ' +  version, function() {
      setup(function() {
        var deflate = spdy.utils.createDeflate(version),
            inflate = spdy.utils.createInflate(version);

        parser = new spdy.parser.create({
          socket: {
            setNoDelay: function() {}
          },
          write: function() {}
        }, deflate, inflate);

        parser.createFramer(version);
      });

      test('should wait for headers initially', function() {
        assert.equal(parser.waiting, 8);
      });

      test('should update buffered property once given < 8 bytes', function() {
        parser.write(new Buffer(5));
        assert.equal(parser.buffered, 5);
      });

      test('given SYN_STREAM header should start waiting for body', function() {
        parser.write(new Buffer([
          0x80, 0x02, 0x00, 0x01, // Control frame, version, type (SYN_STREAM)
          0x00, 0x00, 0x12, 0x34
        ]));

        assert.equal(parser.waiting, 0x1234);
        assert.equal(parser.state.type, 'frame-body');
        assert.ok(parser.state.header.control);
        assert.equal(parser.state.header.flags, 0);
        assert.equal(parser.state.header.length, 0x1234);
      });

      test('given DATA header should start waiting for body', function() {
        parser.write(new Buffer([
          0x00, 0x00, 0x00, 0x01, // Data frame, stream ID
          0x00, 0x00, 0x12, 0x34
        ]));

        assert.equal(parser.waiting, 0x1234);
        assert.equal(parser.state.type, 'frame-body');
        assert.ok(!parser.state.header.control);
        assert.equal(parser.state.header.id, 1);
        assert.equal(parser.state.header.flags, 0);
        assert.equal(parser.state.header.length, 0x1234);
      });

      test('given chunked header should not fail', function() {
        parser.write(new Buffer([
          0x80, 0x02, 0x00, 0x01 // Control frame, version, type (SYN_STREAM)
        ]));
        assert.equal(parser.buffered, 4);

        parser.write(new Buffer([
          0x00, 0x00, 0x12, 0x34
        ]));
        assert.equal(parser.buffered, 0);

        assert.equal(parser.waiting, 0x1234);
        assert.equal(parser.state.type, 'frame-body');
        assert.ok(parser.state.header.control);
        assert.equal(parser.state.header.flags, 0);
        assert.equal(parser.state.header.length, 0x1234);
      });

      test('given header and body should emit `frame`', function(done) {
        parser.on('frame', function(frame) {
          assert.ok(frame.type === 'DATA');
          assert.equal(frame.id, 1);
          assert.equal(frame.data.length, 4);
          assert.equal(frame.data[0], 0x01);
          assert.equal(frame.data[1], 0x02);
          assert.equal(frame.data[2], 0x03);
          assert.equal(frame.data[3], 0x04);
          done();
        });

        parser.write(new Buffer([
          0x00, 0x00, 0x00, 0x01, // Data frame, stream ID
          0x00, 0x00, 0x00, 0x04,
          0x01, 0x02, 0x03, 0x04  // Body
        ]));

        // Waits for next frame
        assert.equal(parser.waiting, 8);
        assert.equal(parser.state.type, 'frame-head');
      });
    });
  });
});
