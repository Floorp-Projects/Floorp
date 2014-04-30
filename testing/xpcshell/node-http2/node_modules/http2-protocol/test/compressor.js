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
  buffer: new Buffer('89e7cf9bfc1ad7d4db7f', 'hex')
}, {
  string: 'éáűőúöüó€',
  buffer: new Buffer('13c3a9c3a1c5b1c591c3bac3b6c3bcc3b3e282ac', 'hex')
}];

test_huffman_request = {
  'GET': 'd5df47',
  'http': 'adcebf',
  '/': '3f',
  'www.foo.com': 'e7cf9bfc1ad7d4db7f',
  'https': 'adcebf1f',
  'www.bar.com': 'e7cf9bfbd383ea6dbf',
  'no-cache': 'b9b9949556bf',
  '/custom-path.css': '3ab8e2e6db9af4bab7d58e3f',
  'custom-key': '571c5cdb737b2faf',
  'custom-value': '571c5cdb73724d9c57'
};

test_huffman_response = {
  '302': '4017',
  'private': 'bf06724b97',
  'Mon, 21 OCt 2013 20:13:21 GMT': 'd6dbb29884de3dce3100a0c4130a262136ad747f',
  ': https://www.bar.com': '98d5b9d7e331cfcf9f37f7a707d4db7f',
  '200': '200f',
  'Mon, 21 OCt 2013 20:13:22 GMT': 'd6dbb29884de3dce3100a0c4130a262236ad747f',
  'https://www.bar.com': 'adcebf198e7e7cf9bfbd383ea6db',
  'gzip': 'abdd97ff',
  'foo=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\
AAAAAAAAAAAAAAAAAAAAAAAAAALASDJKHQKBZXOQWEOPIUAXQWEOIUAXLJKHQWOEIUAL\
QWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234LASDJKHQKBZXOQWEOPIUAXQWEOIUAXLJKH\
QWOEIUALQWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234LASDJKHQKBZXOQWEOPIUAXQWEO\
IUAXLJKHQWOEIUALQWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234LASDJKHQKBZXOQWEOP\
IUAXQWEOIUAXLJKHQWOEIUALQWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234ZZZZZZZZZZ\
ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ1234 m\
ax-age=3600; version=1': 'e0d6cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cff5cfb747cfe9f2fb7d3b7f7e9e3f6fcf7f8f97879e7f4fb7e7bfc7c3cf3fa7d7e7f4f97dbf3e3dfe1e79febf6fcf7f8f879e7f4fafdbbfcf3fa7d7ebf4f9e7dba3edf9eff1f3f0cfe9b049107d73edd1f3fa7cbedf4edfdfa78fdbf3dfe3e5e1e79fd3edf9eff1f0f3cfe9f5f9fd3e5f6fcf8f7f879e7fafdbf3dfe3e1e79fd3ebf6eff3cfe9f5fafd3e79f6e8fb7e7bfc7cfc33fa6c12441f5cfb747cfe9f2fb7d3b7f7e9e3f6fcf7f8f97879e7f4fb7e7bfc7c3cf3fa7d7e7f4f97dbf3e3dfe1e79febf6fcf7f8f879e7f4fafdbbfcf3fa7d7ebf4f9e7dba3edf9eff1f3f0cfe9b049107d73edd1f3fa7cbedf4edfdfa78fdbf3dfe3e5e1e79fd3edf9eff1f0f3cfe9f5f9fd3e5f6fcf8f7f879e7fafdbf3dfe3e1e79fd3ebf6eff3cfe9f5fafd3e79f6e8fb7e7bfc7cfc33fa6c12441fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff624880d6a7a664d4b9d1100761b92f0c58dba71',
  'foo=ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ\
ZZZZZZZZZZZZZZZZZZZZZZZZZZLASDJKHQKBZXOQWEOPIUAXQWEOIUAXLJKHQWOEIUAL\
QWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234LASDJKHQKBZXOQWEOPIUAXQWEOIUAXLJKH\
QWOEIUALQWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234LASDJKHQKBZXOQWEOPIUAXQWEO\
IUAXLJKHQWOEIUALQWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234LASDJKHQKBZXOQWEOP\
IUAXQWEOIUAXLJKHQWOEIUALQWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234AAAAAAAAAA\
AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA1234 m\
ax-age=3600; version=1': 'e0d6cffbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7f5cfb747cfe9f2fb7d3b7f7e9e3f6fcf7f8f97879e7f4fb7e7bfc7c3cf3fa7d7e7f4f97dbf3e3dfe1e79febf6fcf7f8f879e7f4fafdbbfcf3fa7d7ebf4f9e7dba3edf9eff1f3f0cfe9b049107d73edd1f3fa7cbedf4edfdfa78fdbf3dfe3e5e1e79fd3edf9eff1f0f3cfe9f5f9fd3e5f6fcf8f7f879e7fafdbf3dfe3e1e79fd3ebf6eff3cfe9f5fafd3e79f6e8fb7e7bfc7cfc33fa6c12441f5cfb747cfe9f2fb7d3b7f7e9e3f6fcf7f8f97879e7f4fb7e7bfc7c3cf3fa7d7e7f4f97dbf3e3dfe1e79febf6fcf7f8f879e7f4fafdbbfcf3fa7d7ebf4f9e7dba3edf9eff1f3f0cfe9b049107d73edd1f3fa7cbedf4edfdfa78fdbf3dfe3e5e1e79fd3edf9eff1f0f3cfe9f5f9fd3e5f6fcf8f7f879e7fafdbf3dfe3e1e79fd3ebf6eff3cfe9f5fafd3e79f6e8fb7e7bfc7cfc33fa6c124419f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7cf9f3e7ce24880d6a7a664d4b9d1100761b92f0c58dba71'
};

var test_headers = [{
  // literal w/index, name index
  header: {
    name: 1,
    value: 'GET',
    index: true,
    mustNeverIndex: false,
    contextUpdate: false,
    clearReferenceSet: false,
    newMaxSize: 0
  },
  buffer: new Buffer('42' + '03474554', 'hex')
}, {
  // literal w/index, name index
  header: {
    name: 6,
    value: 'http',
    index: true,
    mustNeverIndex: false,
    contextUpdate: false,
    clearReferenceSet: false,
    newMaxSize: 0
  },
  buffer: new Buffer('47' + '83ADCEBF', 'hex')
}, {
  // literal w/index, name index
  header: {
    name: 5,
    value: '/',
    index: true,
    mustNeverIndex: false,
    contextUpdate: false,
    clearReferenceSet: false,
    newMaxSize: 0
  },
  buffer: new Buffer('46' + '012F', 'hex')
}, {
  // literal w/index, name index
  header: {
    name: 3,
    value: 'www.foo.com',
    index: true,
    mustNeverIndex: false,
    contextUpdate: false,
    clearReferenceSet: false,
    newMaxSize: 0
  },
  buffer: new Buffer('44' + '89E7CF9BFC1AD7D4DB7F', 'hex')
}, {
  // literal w/index, name index
  header: {
    name: 2,
    value: 'https',
    index: true,
    mustNeverIndex: false,
    contextUpdate: false,
    clearReferenceSet: false,
    newMaxSize: 0
  },
  buffer: new Buffer('43' + '84ADCEBF1F', 'hex')
}, {
  // literal w/index, name index
  header: {
    name: 1,
    value: 'www.bar.com',
    index: true,
    mustNeverIndex: false,
    contextUpdate: false,
    clearReferenceSet: false,
    newMaxSize: 0
  },
  buffer: new Buffer('42' + '89E7CF9BFBD383EA6DBF', 'hex')
}, {
  // literal w/index, name index
  header: {
    name: 29,
    value: 'no-cache',
    index: true,
    mustNeverIndex: false,
    contextUpdate: false,
    clearReferenceSet: false,
    newMaxSize: 0
  },
  buffer: new Buffer('5e' + '86B9B9949556BF', 'hex')
}, {
  // indexed
  header: {
    name: 3,
    value: 3,
    index: false,
    mustNeverIndex: false,
    contextUpdate: false,
    clearReferenceSet: false,
    newMaxSize: 0
  },
  buffer: new Buffer('84', 'hex')
}, {
  // indexed
  header: {
    name: 5,
    value: 5,
    index: false,
    mustNeverIndex: false,
    contextUpdate: false,
    clearReferenceSet: false,
    newMaxSize: 0
  },
  buffer: new Buffer('86', 'hex')
}, {
  // literal w/index, name index
  header: {
    name: 4,
    value: '/custom-path.css',
    index: true,
    mustNeverIndex: false,
    contextUpdate: false,
    clearReferenceSet: false,
    newMaxSize: 0
  },
  buffer: new Buffer('45' + '8C3AB8E2E6DB9AF4BAB7D58E3F', 'hex')
}, {
  // literal w/index, new name & value
  header: {
    name: 'custom-key',
    value: 'custom-value',
    index: true,
    mustNeverIndex: false,
    contextUpdate: false,
    clearReferenceSet: false,
    newMaxSize: 0
  },
  buffer: new Buffer('40' + '88571C5CDB737B2FAF' + '89571C5CDB73724D9C57', 'hex')
}, {
  // indexed
  header: {
    name: 2,
    value: 2,
    index: false,
    mustNeverIndex: false,
    contextUpdate: false,
    clearReferenceSet: false,
    newMaxSize: 0
  },
  buffer: new Buffer('83', 'hex')
}, {
  // indexed
  header: {
    name: 6,
    value: 6,
    index: false,
    mustNeverIndex: false,
    contextUpdate: false,
    clearReferenceSet: false,
    newMaxSize: 0
  },
  buffer: new Buffer('87', 'hex')
}, {
  // Literal w/o index, name index
  header: {
    name: 6,
    value: "whatever",
    index: false,
    mustNeverIndex: false,
    contextUpdate: false,
    clearReferenceSet: false,
    newMaxSize: 0
  },
  buffer: new Buffer('07' + '86E75A5CBE4BC3', 'hex')
}, {
  // Literal w/o index, new name & value
  header: {
    name: "foo",
    value: "bar",
    index: false,
    mustNeverIndex: false,
    contextUpdate: false,
    clearReferenceSet: false,
    newMaxSize: 0
  },
  buffer: new Buffer('00' + '03666F6F' + '03626172', 'hex')
}, {
  // Literal never indexed, name index
  header: {
    name: 6,
    value: "whatever",
    index: false,
    mustNeverIndex: true,
    contextUpdate: false,
    clearReferenceSet: false,
    newMaxSize: 0
  },
  buffer: new Buffer('17' + '86E75A5CBE4BC3', 'hex')
}, {
  // Literal never indexed, new name & value
  header: {
    name: "foo",
    value: "bar",
    index: false,
    mustNeverIndex: true,
    contextUpdate: false,
    clearReferenceSet: false,
    newMaxSize: 0
  },
  buffer: new Buffer('10' + '03666F6F' + '03626172', 'hex')
}, {
  header: {
    name: -1,
    value: -1,
    index: false,
    mustNeverIndex: false,
    contextUpdate: true,
    clearReferenceSet: true,
    newMaxSize: 0
  },
  buffer: new Buffer('30', 'hex')
}, {
  header: {
    name: -1,
    value: -1,
    index: false,
    mustNeverIndex: false,
    contextUpdate: true,
    clearReferenceSet: false,
    newMaxSize: 100
  },
  buffer: new Buffer('2F55', 'hex')
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
    'cookie': 'first; second; third; third; fourth',
    'multiple': ['first', 'second', 'third', 'third; fourth'],
    'verylong': (new Buffer(9000)).toString('hex')
  }
}];

describe('compressor.js', function() {
  describe('HeaderTable', function() {
  });

  describe('HuffmanTable', function() {
    describe('method encode(buffer)', function() {
      it('should return the Huffman encoded version of the input buffer', function() {
        var table = HuffmanTable.huffmanTable;
        for (var decoded in test_huffman_request) {
          var encoded = test_huffman_request[decoded];
          expect(table.encode(new Buffer(decoded)).toString('hex')).to.equal(encoded);
        }
        table = HuffmanTable.huffmanTable;
        for (decoded in test_huffman_response) {
          encoded = test_huffman_response[decoded];
          expect(table.encode(new Buffer(decoded)).toString('hex')).to.equal(encoded);
        }
      });
    })
    describe('method decode(buffer)', function() {
      it('should return the Huffman decoded version of the input buffer', function() {
        var table = HuffmanTable.huffmanTable;
        for (var decoded in test_huffman_request) {
          var encoded = test_huffman_request[decoded];
          expect(table.decode(new Buffer(encoded, 'hex')).toString()).to.equal(decoded)
        }
        table = HuffmanTable.huffmanTable;
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
        var table = HuffmanTable.huffmanTable;
        for (var i = 0; i < test_strings.length; i++) {
          var test = test_strings[i];
          expect(util.concat(HeaderSetCompressor.string(test.string, table))).to.deep.equal(test.buffer);
        }
      });
    });
    describe('static method .header({ name, value, index })', function() {
      it('should return an array of buffers that represent the encoded form of the header', function() {
        var table = HuffmanTable.huffmanTable;
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
        var table = HuffmanTable.huffmanTable;
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
        var table = HuffmanTable.huffmanTable;
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
        for (var i = 0; i < test_header_sets.length - 1; i++) {
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
          var table = HuffmanTable.huffmanTable;
          var result = table.decode(table.encode(buffer));
          expect(result).to.deep.equal(buffer);
        }
      })
    })
  });
});
