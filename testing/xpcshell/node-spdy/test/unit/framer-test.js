var assert = require('assert'),
    spdy = require('../../'),
    Buffer = require('buffer').Buffer,
    Stream = require('stream').Stream;

suite('A Framer of SPDY module', function() {
  var inflate,
      deflate,
      framer;

  setup(function() {
    inflate = spdy.utils.zwrap(spdy.utils.createInflate(2));
    deflate = spdy.utils.zwrap(spdy.utils.createDeflate(2));
    framer = new spdy.protocol[2].Framer(deflate, inflate);
  });

  /*
    deflate.on('data', function(b) {console.log(b)});
    deflate.write(new Buffer([
      0x00, 0x02, // Number of name+value
      0, 0x04, // Name length
      0x68, 0x6f, 0x73, 0x74, // 'host'
      0, 0x09, // Value length
      0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x68, 0x6f, 0x73, 0x74, // 'localhost'
      0, 0x06,
      0x63, 0x75, 0x73, 0x74, 0x6f, 0x6d, // 'custom',
      0, 0x1,
      0x31 // '1'
    ]));
    deflate.flush();
   */

  suite('frame parsing', function() {
    test('given a SYN_STREAM should return correct frame', function(done) {
      var body = new Buffer([
        0x00, 0x00, 0x00, 0x01, // Stream ID
        0x00, 0x00, 0x00, 0x00, // Associated Stream ID
        0x00, 0x00, // Priority + Unused
        0x78, 0xbb, 0xdf, 0xa2, 0x51, 0xb2, // Deflated Name/Value pairs
        0x62, 0x60, 0x62, 0x60, 0x01, 0xe5, 0x12,
        0x06, 0x4e, 0x50, 0x50, 0xe6, 0x80, 0x99,
        0x6c, 0xc9, 0xa5, 0xc5, 0x25, 0xf9, 0xb9,
        0x0c, 0x8c, 0x86, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff
      ]);
      framer.execute({
        control: true,
        type: 1,
        length: body.length
      }, body, function(err, frame) {
        assert.ok(!err);
        assert.equal(frame.type, 'SYN_STREAM');
        assert.equal(frame.id, 1);
        assert.equal(frame.associated, 0);
        assert.equal(frame.headers.host, 'localhost');
        assert.equal(frame.headers.custom, '1');
        done();
      });
    });

    test('given a SYN_REPLY should return correct frame', function(done) {
      var body = new Buffer([
        0x00, 0x00, 0x00, 0x01, // Stream ID
        0x00, 0x00, // Unused
        0x78, 0xbb, 0xdf, 0xa2, 0x51, 0xb2, // Deflated Name/Value pairs
        0x62, 0x60, 0x62, 0x60, 0x01, 0xe5, 0x12,
        0x06, 0x4e, 0x50, 0x50, 0xe6, 0x80, 0x99,
        0x6c, 0xc9, 0xa5, 0xc5, 0x25, 0xf9, 0xb9,
        0x0c, 0x8c, 0x86, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff
      ]);
      framer.execute({
        control: true,
        type: 2,
        length: body.length
      }, body, function(err, frame) {
        assert.ok(!err);
        assert.equal(frame.type, 'SYN_REPLY');
        assert.equal(frame.id, 1);
        assert.equal(frame.headers.host, 'localhost');
        assert.equal(frame.headers.custom, '1');
        done();
      });
    });

    test('given a RST_STREAM should return correct frame', function(done) {
      var body = new Buffer([0, 0, 0, 1, 0, 0, 0, 2]);
      framer.execute({
        control: true,
        type: 3,
        length: body.length
      }, body, function(err, frame) {
        assert.ok(!err);
        assert.equal(frame.type, 'RST_STREAM');
        assert.equal(frame.id, 1);
        assert.equal(frame.status, 2);
        done();
      });
    });

    test('given a NOOP frame should return correct frame', function(done) {
      framer.execute({
        control: true,
        type: 5,
        length: 0
      }, new Buffer(0), function(err, frame) {
        assert.ok(!err);
        assert.equal(frame.type, 'NOOP');
        done();
      });
    });

    test('given a PING frame should return correct frame', function(done) {
      framer.execute({
        control: true,
        type: 6,
        length: 0
      }, new Buffer(0), function(err, frame) {
        assert.ok(!err);
        assert.equal(frame.type, 'PING');
        done();
      });
    });

    test('given a GOAWAY frame should return correct frame', function(done) {
      var body = new Buffer([0, 0, 0, 1]);
      framer.execute({
        control: true,
        type: 7,
        length: body.length
      }, body, function(err, frame) {
        assert.ok(!err);
        assert.equal(frame.type, 'GOAWAY');
        assert.equal(frame.lastId, 1);
        done();
      });
    });
  });

  suite('frame generation', function() {
    test('.replyFrame() should generate correct frame', function(done) {
      framer.replyFrame(1, 200, 'ok', {}, function(err, chunks) {
        assert.equal(err, null);
        assert.ok(chunks.length > 1);
        done();
      });
    });

    test('.streamFrame() should generate correct frame', function(done) {
      framer.streamFrame(2, 1, { url : '/' }, {}, function(err, chunks) {
        assert.equal(err, null);
        assert.ok(chunks.length > 1);
        done();
      });
    });

    test('.dataFrame() w/o fin should generate correct frame', function() {
      var frame = framer.dataFrame(1, false, new Buffer(123));
      assert.equal(frame[4], 0);
      assert.ok(frame.length > 8);
    });

    test('.dataFrame() with fin should generate correct frame', function() {
      var frame = framer.dataFrame(1, true, new Buffer(123));
      assert.equal(frame[4], 1);
      assert.ok(frame.length > 8);
    });

    test('.pingFrame() should generate correct frame', function() {
      var frame = framer.pingFrame(new Buffer([0, 1, 2, 3]));
      assert.ok(frame.length > 0);
    });

    test('.rstFrame() should generate correct frame', function() {
      var frame = framer.rstFrame(1, 2);
      assert.ok(frame.length > 0);

      // Verify that cache works
      var frame = framer.rstFrame(1, 2);
      assert.ok(frame.length > 0);
    });
  });
});
