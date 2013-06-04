var v3;

try {
  v3 = require('./protocol.node');
} catch (e) {
  v3 = require('./protocol.js');
}
module.exports = v3;

v3.Framer = require('./framer').Framer;

v3.dictionary = require('./dictionary').dictionary;
