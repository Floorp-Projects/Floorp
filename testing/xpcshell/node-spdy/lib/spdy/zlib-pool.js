var zlibpool = exports,
    spdy = require('../spdy');

//
// ### function Pool ()
// Zlib streams pool
//
function Pool() {
  this.pool = {
    'spdy/2': [],
    'spdy/3': []
  };
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
Pool.prototype.get = function get(version, callback) {
  if (this.pool[version].length > 0) {
    return this.pool[version].pop();
  } else {
    var id = version.split('/', 2)[1];

    return {
      version: version,
      deflate: spdy.utils.createDeflate(id),
      inflate: spdy.utils.createInflate(id)
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
      self.pool[pair.version].push(pair);
    }
  }
};
