var spdy = exports;

// Exports utils
spdy.utils = require('./spdy/utils');

// Export parser&framer
spdy.protocol = require('./spdy/protocol');

// Export ServerResponse
spdy.response = require('./spdy/response');

// Export Scheduler
spdy.scheduler = require('./spdy/scheduler');

// Export ZlibPool
spdy.zlibpool = require('./spdy/zlib-pool');

// Export Connection and Stream
spdy.Stream = require('./spdy/stream').Stream;
spdy.Connection = require('./spdy/connection').Connection;

// Export server
spdy.server = require('./spdy/server');
spdy.Server = spdy.server.Server;
spdy.createServer = spdy.server.create;

// Export client
spdy.Agent = require('./spdy/client').Agent;
spdy.createAgent = require('./spdy/client').create;
