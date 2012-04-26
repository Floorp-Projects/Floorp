var zlibpool = exports,
    spdy = require('../spdy');

//
// ### function Pool ()
// Zlib streams pool
//
function Pool() {
  this.pool = [];
}

//
// ### function create ()
// Returns instance of Pool
//
zlibpool.create = function create() {
  return new Pool();
};

var x = 0;
//
// ### function get ()
// Returns pair from pool or a new one
//
Pool.prototype.get = function get(callback) {
  if (this.pool.length > 0) {
    return this.pool.pop();
  } else {
    return {
      deflate: spdy.utils.createDeflate(),
      inflate: spdy.utils.createInflate()
    };
  }
};

//
// ### function put (pair)
// Puts pair into pool
//
Pool.prototype.put = function put(pair) {
  var self = this,
      waiting = 2;

  spdy.utils.resetZlibStream(pair.inflate, done);
  spdy.utils.resetZlibStream(pair.deflate, done);

  function done() {
    if (--waiting === 0) {
      self.pool.push(pair);
    }
  }
};
