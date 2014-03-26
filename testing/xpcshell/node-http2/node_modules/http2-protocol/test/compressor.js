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
  '302': '98a7',
  'private': '73d5cd111f',
  'Mon, 21 OCt 2013 20:13:21 GMT': 'ef6b3a7a0e6e8fa7647a0e534dd072fb0d37b0e6e8f777f8ff',
  ': https://www.bar.com': 'f6746718ba1ec00db6d897a1e44b74',
  '200': '394b',
  'Mon, 21 OCt 2013 20:13:22 GMT': 'ef6b3a7a0e6e8fa7647a0e534dd072fb0d37b0e7e8f777f8ff',
  'https://www.bar.com': 'ce31743d801b6db12f43c896e9',
  'gzip': 'cbd54e',
  'foo=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\
AAAAAAAAAAAAAAAAAAAAAAAAAALASDJKHQKBZXOQWEOPIUAXQWEOIUAXLJKHQWOEIUAL\
QWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234LASDJKHQKBZXOQWEOPIUAXQWEOIUAXLJKH\
QWOEIUALQWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234LASDJKHQKBZXOQWEOPIUAXQWEO\
IUAXLJKHQWOEIUALQWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234LASDJKHQKBZXOQWEOP\
IUAXQWEOIUAXLJKHQWOEIUALQWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234ZZZZZZZZZZ\
ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ1234 m\
ax-age=3600; version=1': 'c5adb77efdfbf7efdfbf7efdfbf7efdfbf7efdfbf7\
efdfbf7efdfbf7efdfbf7efdfbf7efdfbf7efdfbf7efdfbf7efdfbf7efdfbf7efdfb\
f7efdfbf7efdfbf7efdfbfe5bfc3b7e3fdfbfedfdf5ff9fbfa7dbf5ddf4fafc3f1bf\
f7f6fd777d3e1f8dffbf97c7fbf7fdbf5f4eef87e37fcbedfaeefa7c3f1bff7f2fb7\
77e37fefe5f2fefe3bfc3b7edfaeefa7e3e1bff7f331e69fe5bfc3b7e3fdfbfedfdf\
5ff9fbfa7dbf5ddf4fafc3f1bff7f6fd777d3e1f8dffbf97c7fbf7fdbf5f4eef87e3\
7fcbedfaeefa7c3f1bff7f2fb777e37fefe5f2fefe3bfc3b7edfaeefa7e3e1bff7f3\
31e69fe5bfc3b7e3fdfbfedfdf5ff9fbfa7dbf5ddf4fafc3f1bff7f6fd777d3e1f8d\
ffbf97c7fbf7fdbf5f4eef87e37fcbedfaeefa7c3f1bff7f2fb777e37fefe5f2fefe\
3bfc3b7edfaeefa7e3e1bff7f331e69fe5bfc3b7e3fdfbfedfdf5ff9fbfa7dbf5ddf\
4fafc3f1bff7f6fd777d3e1f8dffbf97c7fbf7fdbf5f4eef87e37fcbedfaeefa7c3f\
1bff7f2fb777e37fefe5f2fefe3bfc3b7edfaeefa7e3e1bff7f331e69ffcff3fcff3\
fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcf\
f3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3f\
cff3fcff3fcff3fcff3fcff3fcff3fcff0c79a7e8d11e72a321b66a4a5eae8e62f82\
9acb4d',
  'foo=ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ\
ZZZZZZZZZZZZZZZZZZZZZZZZZZLASDJKHQKBZXOQWEOPIUAXQWEOIUAXLJKHQWOEIUAL\
QWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234LASDJKHQKBZXOQWEOPIUAXQWEOIUAXLJKH\
QWOEIUALQWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234LASDJKHQKBZXOQWEOPIUAXQWEO\
IUAXLJKHQWOEIUALQWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234LASDJKHQKBZXOQWEOP\
IUAXQWEOIUAXLJKHQWOEIUALQWEOIUAXLQEUAXLLKJASDQWEOUIAXN1234AAAAAAAAAA\
AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA1234 m\
ax-age=3600; version=1': 'c5adb7fcff3fcff3fcff3fcff3fcff3fcff3fcff3f\
cff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff\
3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fcff3fc\
ff3fcff3e5bfc3b7e3fdfbfedfdf5ff9fbfa7dbf5ddf4fafc3f1bff7f6fd777d3e1f\
8dffbf97c7fbf7fdbf5f4eef87e37fcbedfaeefa7c3f1bff7f2fb777e37fefe5f2fe\
fe3bfc3b7edfaeefa7e3e1bff7f331e69fe5bfc3b7e3fdfbfedfdf5ff9fbfa7dbf5d\
df4fafc3f1bff7f6fd777d3e1f8dffbf97c7fbf7fdbf5f4eef87e37fcbedfaeefa7c\
3f1bff7f2fb777e37fefe5f2fefe3bfc3b7edfaeefa7e3e1bff7f331e69fe5bfc3b7\
e3fdfbfedfdf5ff9fbfa7dbf5ddf4fafc3f1bff7f6fd777d3e1f8dffbf97c7fbf7fd\
bf5f4eef87e37fcbedfaeefa7c3f1bff7f2fb777e37fefe5f2fefe3bfc3b7edfaeef\
a7e3e1bff7f331e69fe5bfc3b7e3fdfbfedfdf5ff9fbfa7dbf5ddf4fafc3f1bff7f6\
fd777d3e1f8dffbf97c7fbf7fdbf5f4eef87e37fcbedfaeefa7c3f1bff7f2fb777e3\
7fefe5f2fefe3bfc3b7edfaeefa7e3e1bff7f331e69f7efdfbf7efdfbf7efdfbf7ef\
dfbf7efdfbf7efdfbf7efdfbf7efdfbf7efdfbf7efdfbf7efdfbf7efdfbf7efdfbf7\
efdfbf7efdfbf7efdfbf7efdfbf7efdfbcc79a7e8d11e72a321b66a4a5eae8e62f82\
9acb4d'
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
  buffer: new Buffer('84', 'hex')
}, {
  header: {
    name: 5,
    value: 5,
    index: false
  },
  buffer: new Buffer('86', 'hex')
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
  buffer: new Buffer('83', 'hex')
}, {
  header: {
    name: 6,
    value: 6,
    index: false
  },
  buffer: new Buffer('87', 'hex')
}, {
  header: {
    name: -1,
    value: -1,
    index: true
  },
  buffer: new Buffer('8080', 'hex')
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
  headers: {},
  buffer: test_headers[13].buffer
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
        for (var i = 0; i < 5; i++) {
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
