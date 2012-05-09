var v2;

try {
  v2 = require('./protocol.node');
} catch (e) {
  v2 = require('./protocol.js');
}
module.exports = v2;

v2.Framer = require('./framer').Framer;
