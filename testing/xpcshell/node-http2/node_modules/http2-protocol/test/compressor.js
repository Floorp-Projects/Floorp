var expect = require('chai').expect;
var util = require('./util');

var compressor = require('../lib/compressor');
var HeaderTable = compressor.HeaderTable;
var HuffmanTable = compressor.HuffmanTable;
var HeaderSetCompressor = compressor.HeaderSetCompressor;
var HeaderSetDecompressor = compressor.HeaderSetDecompressor;
var Compressor = compressor.Compressor;
var Decompressor = compressor.Decompressor;

var test_integers = [{
  N: 5,
  I: 10,
  buffer: new Buffer([10])
}, {
  N: 0,
  I: 10,
  buffer: new Buffer([10])
}, {
  N: 5,
  I: 1337,
  buffer: new Buffer([31, 128 + 26, 10])
}, {
  N: 0,
  I: 1337,
  buffer: new Buffer([128 + 57, 10])
}];

var test_strings = [{
  string: 'www.foo.com',
  buffer: new Buffer('88db6d898b5a44b74f', 'hex')
}, {
  string: 'éáűőúöüó€',
  buffer: new Buffer('13C3A9C3A1C5B1C591C3BAC3B6C3BCC3B3E282AC', 'hex')
}];

test_huffman_request = {
  'GET': 'f77778ff',
  'http': 'ce3177',
  '/': '0f',
  'www.foo.com': 'db6d898b5a44b74f',
  'https': 'ce31743f',
  'www.bar.com': 'db6d897a1e44b74f',
  'no-cache': '63654a1398ff',
  '/custom-path.css': '04eb08b7495c88e644c21f',
  'custom-key': '4eb08b749790fa7f',
  'custom-value': '4eb08b74979a17a8ff'
};

test_huffman_response = {
  '302': '409f',
  'private': 'c31b39bf387f',
  'Mon, 21 OCt 2013 20:13:21 GMT': 'a2fba20320f2ebcc0c490062d2434c827a1d',
  ': https://www.bar.com': '6871cf3c326ebd7e9e9e926e7e32557dbf',
  '200': '311f',
  'Mon, 21 OCt 2013 20:13:22 GMT': 'a2fba20320f2ebcc0c490062d2434cc27a1d',
  'https://www.bar.com': 'e39e7864dd7afd3d3d24dcfc64aafb7f',
  'gzip': 'e1fbb30f',
  'foo=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\
AAAAAAAAAAAAAAAAAAAAAAAAAALASDJKHQKBZXOQWEOPIUAXQWEOIUAXLJKHQWOEIUAL\
QWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234LASDJKHQKBZXOQWEOPIUAXQWEOIUAXLJKH\
QWOEIUALQWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234LASDJKHQKBZXOQWEOPIUAXQWEO\
IUAXLJKHQWOEIUALQWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234LASDJKHQKBZXOQWEOP\
IUAXQWEOIUAXLJKHQWOEIUALQWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234ZZZZZZZZZZ\
ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ1234 m\
ax-age=3600; version=1': 'df7dfb36eddbb76eddbb76eddbb76eddbb76eddbb76eddbb76\
eddbb76eddbb76eddbb76eddbb76eddbb76eddbb76eddbb76eddbb76eddbb76eddbb\
76eddbb76eddbb7e3b69ecf0fe7e1fd7f3d5fe7f7e5fd79f6f97cbbfe9b7fbfebcfb\
7cbbfe9b7fbf8f87f3f0febcfcbb7bfe9b7e3fd79f6f977fd36ff7f1febb7e9b7fbf\
8fc7f9f0db4f67f5e7dbe5f4efdbfdf891a13f1db4f6787f3f0febf9eaff3fbf2feb\
cfb7cbe5dff4dbfdff5e7dbe5dff4dbfdfc7c3f9f87f5e7e5dbdff4dbf1febcfb7cb\
bfe9b7fbf8ff5dbf4dbfdfc7e3fcf86da7b3faf3edf2fa77edfefc48d09f8eda7b3c\
3f9f87f5fcf57f9fdf97f5e7dbe5f2effa6dfeffaf3edf2effa6dfefe3e1fcfc3faf\
3f2edeffa6df8ff5e7dbe5dff4dbfdfc7faedfa6dfefe3f1fe7c36d3d9fd79f6f97d\
3bf6ff7e24684fc76d3d9e1fcfc3fafe7abfcfefcbfaf3edf2f977fd36ff7fd79f6f\
977fd36ff7f1f0fe7e1fd79f976f7fd36fc7faf3edf2effa6dfefe3fd76fd36ff7f1\
f8ff3e1b69ecfebcfb7cbe9dfb7fbf123427fcff3fcff3fcff3fcff3fcff3fcff3fc\
ff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3\
fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcf\
f3fcff3fcff08d090b5fd237f086c44a23ef0e70c72b2fbb617f',
  'foo=ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ\
ZZZZZZZZZZZZZZZZZZZZZZZZZZLASDJKHQKBZXOQWEOPIUAXQWEOIUAXLJKHQWOEIUAL\
QWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234LASDJKHQKBZXOQWEOPIUAXQWEOIUAXLJKH\
QWOEIUALQWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234LASDJKHQKBZXOQWEOPIUAXQWEO\
IUAXLJKHQWOEIUALQWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234LASDJKHQKBZXOQWEOP\
IUAXQWEOIUAXLJKHQWOEIUALQWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234AAAAAAAAAA\
AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA1234 m\
ax-age=3600; version=1': 'df7dfb3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcf\
f3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3f\
cff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff\
3e3b69ecf0fe7e1fd7f3d5fe7f7e5fd79f6f97cbbfe9b7fbfebcfb7cbbfe9b7fbf8f\
87f3f0febcfcbb7bfe9b7e3fd79f6f977fd36ff7f1febb7e9b7fbf8fc7f9f0db4f67\
f5e7dbe5f4efdbfdf891a13f1db4f6787f3f0febf9eaff3fbf2febcfb7cbe5dff4db\
fdff5e7dbe5dff4dbfdfc7c3f9f87f5e7e5dbdff4dbf1febcfb7cbbfe9b7fbf8ff5d\
bf4dbfdfc7e3fcf86da7b3faf3edf2fa77edfefc48d09f8eda7b3c3f9f87f5fcf57f\
9fdf97f5e7dbe5f2effa6dfeffaf3edf2effa6dfefe3e1fcfc3faf3f2edeffa6df8f\
f5e7dbe5dff4dbfdfc7faedfa6dfefe3f1fe7c36d3d9fd79f6f97d3bf6ff7e24684f\
c76d3d9e1fcfc3fafe7abfcfefcbfaf3edf2f977fd36ff7fd79f6f977fd36ff7f1f0\
fe7e1fd79f976f7fd36fc7faf3edf2effa6dfefe3fd76fd36ff7f1f8ff3e1b69ecfe\
bcfb7cbe9dfb7fbf1234276eddbb76eddbb76eddbb76eddbb76eddbb76eddbb76edd\
bb76eddbb76eddbb76eddbb76eddbb76eddbb76eddbb76eddbb76eddbb76eddbb76e\
ddbb76eddbb48d090b5fd237f086c44a23ef0e70c72b2fbb617f'
};

var test_headers = [{
  header: {
    name: 1,
    value: 'GET',
    index: true
  },
  buffer: new Buffer('02' + '03474554', 'hex')
}, {
  header: {
    name: 6,
    value: 'http',
    index: true
  },
  buffer: new Buffer('07' + '83ce3177', 'hex')
}, {
  header: {
    name: 5,
    value: '/',
    index: true
  },
  buffer: new Buffer('06' + '012f', 'hex')
}, {
  header: {
    name: 3,
    value: 'www.foo.com',
    index: true
  },
  buffer: new Buffer('04' + '88db6d898b5a44b74f', 'hex')
}, {
  header: {
    name: 2,
    value: 'https',
    index: true
  },
  buffer: new Buffer('03' + '84ce31743f', 'hex')
}, {
  header: {
    name: 1,
    value: 'www.bar.com',
    index: true
  },
  buffer: new Buffer('02' + '88db6d897a1e44b74f', 'hex')
}, {
  header: {
    name: 28,
    value: 'no-cache',
    index: true
  },
  buffer: new Buffer('1d' + '8663654a1398ff', 'hex')
}, {
  header: {
    name: 3,
    value: 3,
    index: false
  },
  buffer: new Buffer('83', 'hex')
}, {
  header: {
    name: 5,
    value: 5,
    index: false
  },
  buffer: new Buffer('85', 'hex')
}, {
  header: {
    name: 4,
    value: '/custom-path.css',
    index: true
  },
  buffer: new Buffer('05' + '8b04eb08b7495c88e644c21f', 'hex')
}, {
  header: {
    name: 'custom-key',
    value: 'custom-value',
    index: true
  },
  buffer: new Buffer('00' + '884eb08b749790fa7f' + '894eb08b74979a17a8ff', 'hex')
}, {
  header: {
    name: 2,
    value: 2,
    index: false
  },
  buffer: new Buffer('82', 'hex')
}, {
  header: {
    name: 6,
    value: 6,
    index: false
  },
  buffer: new Buffer('86', 'hex')
}];

var test_header_sets = [{
  headers: {
    ':method': 'GET',
    ':scheme': 'http',
    ':path': '/',
    ':authority': 'www.foo.com'
  },
  buffer: util.concat(test_headers.slice(0, 4).map(function(test) { return test.buffer; }))
}, {
  headers: {
    ':method': 'GET',
    ':scheme': 'https',
    ':path': '/',
    ':authority': 'www.bar.com',
    'cache-control': 'no-cache'
  },
  buffer: util.concat(test_headers.slice(4, 9).map(function(test) { return test.buffer; }))
}, {
  headers: {
    ':method': 'GET',
    ':scheme': 'https',
    ':path': '/custom-path.css',
    ':authority': 'www.bar.com',
    'custom-key': 'custom-value'
  },
  buffer: util.concat(test_headers.slice(9, 13).map(function(test) { return test.buffer; }))
}, {
  headers: {
    ':method': 'GET',
    ':scheme': 'https',
    ':path': '/custom-path.css',
    ':authority': ['www.foo.com', 'www.bar.com'],
    'custom-key': 'custom-value'
  },
  buffer: test_headers[3].buffer
}, {
  headers: {
    ':status': '200',
    'user-agent': 'my-user-agent',
    'cookie': ['first', 'second', 'third', 'third'],
    'verylong': (new Buffer(9000)).toString('hex')
  }
}];

describe('compressor.js', function() {
  describe('HeaderTable', function() {
  });

  describe('HuffmanTable', function() {
    describe('method encode(buffer)', function() {
      it('should return the Huffman encoded version of the input buffer', function() {
        var table = HuffmanTable.requestHuffmanTable;
        for (var decoded in test_huffman_request) {
          var encoded = test_huffman_request[decoded];
          expect(table.encode(new Buffer(decoded)).toString('hex')).to.equal(encoded);
        }
        table = HuffmanTable.responseHuffmanTable;
        for (decoded in test_huffman_response) {
          encoded = test_huffman_response[decoded];
          expect(table.encode(new Buffer(decoded)).toString('hex')).to.equal(encoded);
        }
      });
    })
    describe('method decode(buffer)', function() {
      it('should return the Huffman decoded version of the input buffer', function() {
        var table = HuffmanTable.requestHuffmanTable;
        for (var decoded in test_huffman_request) {
          var encoded = test_huffman_request[decoded];
          expect(table.decode(new Buffer(encoded, 'hex')).toString()).to.equal(decoded)
        }
        table = HuffmanTable.responseHuffmanTable;
        for (decoded in test_huffman_response) {
          encoded = test_huffman_response[decoded];
          expect(table.decode(new Buffer(encoded, 'hex')).toString()).to.equal(decoded)
        }
      });
    })
  });

  describe('HeaderSetCompressor', function() {
    describe('static method .integer(I, N)', function() {
      it('should return an array of buffers that represent the N-prefix coded form of the integer I', function() {
        for (var i = 0; i < test_integers.length; i++) {
          var test = test_integers[i];
          test.buffer.cursor = 0;
          expect(util.concat(HeaderSetCompressor.integer(test.I, test.N))).to.deep.equal(test.buffer);
        }
      });
    });
    describe('static method .string(string)', function() {
      it('should return an array of buffers that represent the encoded form of the string', function() {
        var table = HuffmanTable.requestHuffmanTable;
        for (var i = 0; i < test_strings.length; i++) {
          var test = test_strings[i];
          expect(util.concat(HeaderSetCompressor.string(test.string, table))).to.deep.equal(test.buffer);
        }
      });
    });
    describe('static method .header({ name, value, index })', function() {
      it('should return an array of buffers that represent the encoded form of the header', function() {
        var table = HuffmanTable.requestHuffmanTable;
        for (var i = 0; i < test_headers.length; i++) {
          var test = test_headers[i];
          expect(util.concat(HeaderSetCompressor.header(test.header, table))).to.deep.equal(test.buffer);
        }
      });
    });
  });

  describe('HeaderSetDecompressor', function() {
    describe('static method .integer(buffer, N)', function() {
      it('should return the parsed N-prefix coded number and increase the cursor property of buffer', function() {
        for (var i = 0; i < test_integers.length; i++) {
          var test = test_integers[i];
          test.buffer.cursor = 0;
          expect(HeaderSetDecompressor.integer(test.buffer, test.N)).to.equal(test.I);
          expect(test.buffer.cursor).to.equal(test.buffer.length);
        }
      });
    });
    describe('static method .string(buffer)', function() {
      it('should return the parsed string and increase the cursor property of buffer', function() {
        var table = HuffmanTable.requestHuffmanTable;
        for (var i = 0; i < test_strings.length; i++) {
          var test = test_strings[i];
          test.buffer.cursor = 0;
          expect(HeaderSetDecompressor.string(test.buffer, table)).to.equal(test.string);
          expect(test.buffer.cursor).to.equal(test.buffer.length);
        }
      });
    });
    describe('static method .header(buffer)', function() {
      it('should return the parsed header and increase the cursor property of buffer', function() {
        var table = HuffmanTable.requestHuffmanTable;
        for (var i = 0; i < test_headers.length; i++) {
          var test = test_headers[i];
          test.buffer.cursor = 0;
          expect(HeaderSetDecompressor.header(test.buffer, table)).to.deep.equal(test.header);
          expect(test.buffer.cursor).to.equal(test.buffer.length);
        }
      });
    });
  });
  describe('Decompressor', function() {
    describe('method decompress(buffer)', function() {
      it('should return the parsed header set in { name1: value1, name2: [value2, value3], ... } format', function() {
        var decompressor = new Decompressor(util.log, 'REQUEST');
        for (var i = 0; i < 4; i++) {
          var header_set = test_header_sets[i];
          expect(decompressor.decompress(header_set.buffer)).to.deep.equal(header_set.headers);
        }
      });
    });
    describe('transform stream', function() {
      it('should emit an error event if a series of header frames is interleaved with other frames', function() {
        var decompressor = new Decompressor(util.log, 'REQUEST');
        var error_occured = false;
        decompressor.on('error', function() {
          error_occured = true;
        });
        decompressor.write({
          type: 'HEADERS',
          flags: {
            END_HEADERS: false
          },
          data: new Buffer(5)
        });
        decompressor.write({
          type: 'DATA',
          flags: {},
          data: new Buffer(5)
        });
        expect(error_occured).to.be.equal(true);
      });
    });
  });

  describe('invariant', function() {
    describe('decompressor.decompress(compressor.compress(headerset)) === headerset', function() {
      it('should be true for any header set if the states are synchronized', function() {
        var compressor = new Compressor(util.log, 'REQUEST');
        var decompressor = new Decompressor(util.log, 'REQUEST');
        var n = test_header_sets.length;
        for (var i = 0; i < 10; i++) {
          var headers = test_header_sets[i%n].headers;
          var compressed = compressor.compress(headers);
          var decompressed = decompressor.decompress(compressed);
          expect(decompressed).to.deep.equal(headers);
          expect(compressor._table).to.deep.equal(decompressor._table);
        }
      });
    });
    describe('source.pipe(compressor).pipe(decompressor).pipe(destination)', function() {
      it('should behave like source.pipe(destination) for a stream of frames', function(done) {
        var compressor = new Compressor(util.log, 'RESPONSE');
        var decompressor = new Decompressor(util.log, 'RESPONSE');
        var n = test_header_sets.length;
        compressor.pipe(decompressor);
        for (var i = 0; i < 10; i++) {
          compressor.write({
            type: i%2 ? 'HEADERS' : 'PUSH_PROMISE',
            flags: {},
            headers: test_header_sets[i%n].headers
          });
        }
        setTimeout(function() {
          for (var j = 0; j < 10; j++) {
            expect(decompressor.read().headers).to.deep.equal(test_header_sets[j%n].headers);
          }
          done();
        }, 10);
      });
    });
    describe('huffmanTable.decompress(huffmanTable.compress(buffer)) === buffer', function() {
      it('should be true for any buffer', function() {
        for (var i = 0; i < 10; i++) {
          var buffer = [];
          while (Math.random() > 0.1) {
            buffer.push(Math.floor(Math.random() * 256))
          }
          buffer = new Buffer(buffer);
          var table = HuffmanTable.requestHuffmanTable;
          var result = table.decode(table.encode(buffer));
          expect(result).to.deep.equal(buffer);
        }
      })
    })
  });
});
