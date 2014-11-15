var spdy = require('../spdy');
var utils = spdy.utils;
var scheduler = exports;

//
// ### function Scheduler (connection)
// #### @connection {spdy.Connection} active connection
// Connection's streams scheduler
//
function Scheduler(connection) {
  this.connection = connection;
  this.priorities = [[], [], [], [], [], [], [], []];
  this._tickListener = this._tickListener.bind(this);
  this._tickListening = false;
  this._tickCallbacks = [];
  this._corked = false;
}

//
// ### function create (connection)
// #### @connection {spdy.Connection} active connection
//
exports.create = function create(connection) {
  return new Scheduler(connection);
};

//
// ### function schedule (stream, data)
// #### @stream {spdy.Stream} Source stream
// #### @data {Buffer} data to write on tick
// Use stream priority to invoke callbacks in right order
//
Scheduler.prototype.schedule = function schedule(stream, data) {
  // Ignore data from destroyed stream
  if (stream._spdyState.destroyed)
    return;
  this.scheduleLast(stream, data);
};

//
// ### function scheduleLast (stream, data)
// #### @stream {spdy.Stream} Source stream
// #### @data {Buffer} data to write on tick
// Use stream priority to invoke callbacks in right order
//
Scheduler.prototype.scheduleLast = function scheduleLast(stream, data) {
  var priority = stream._spdyState.priority;
  priority = Math.min(priority, 7);
  priority = Math.max(priority, 0);
  this.priorities[priority].push(data);
};

//
// ### function tickListener ()
//
Scheduler.prototype._tickListener = function tickListener() {
  var priorities = this.priorities;
  var tickCallbacks = this._tickCallbacks;

  this._tickListening = false;
  this.priorities = [[], [], [], [], [], [], [], []];
  this._tickCallbacks = [];

  // Run all priorities
  for (var i = 0; i < 8; i++)
    for (var j = 0; j < priorities[i].length; j++)
      this.connection.write(priorities[i][j]);

  // Invoke callbacks
  for (var i = 0; i < tickCallbacks.length; i++)
    tickCallbacks[i]();
  if (this._corked) {
    this.connection.uncork();
    if (this._tickListening)
      this.connection.cork();
    else
      this._corked = false;
  }
};

//
// ### function tick ()
// Add .nextTick callback if not already present
//
Scheduler.prototype.tick = function tick(cb) {
  if (cb)
    this._tickCallbacks.push(cb);
  if (this._tickListening)
    return;
  this._tickListening = true;

  if (!this._corked) {
    this.connection.cork();
    this._corked = true;
  }
  if (!this.connection._spdyState.parser.needDrain) {
    utils.nextTick(this._tickListener);
  } else {
    this.connection._spdyState.parser.once('drain', this._tickListener);
  }
};
