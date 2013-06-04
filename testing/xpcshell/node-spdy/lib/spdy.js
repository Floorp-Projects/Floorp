var spdy = exports;

// Exports utils
spdy.utils = require('./spdy/utils');

// Export parser&framer
spdy.protocol = {};

try {
  spdy.protocol.generic = require('./spdy/protocol/generic.node');
} catch (e) {
  spdy.protocol.generic = require('./spdy/protocol/generic.js');
}

// Supported SPDY versions
spdy.protocol[2] = require('./spdy/protocol/v2');
spdy.protocol[3] = require('./spdy/protocol/v3');

spdy.parser = require('./spdy/parser');

// Export ServerResponse
spdy.response = require('./spdy/response');

// Export Scheduler
spdy.scheduler = require('./spdy/scheduler');

// Export ZlibPool
spdy.zlibpool = require('./spdy/zlib-pool');

// Export server
spdy.server = require('./spdy/server');
spdy.createServer = spdy.server.create;
