/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

'use strict';

var api = require('../api');
var Commands = require('../commands/commands').Commands;
var Types = require('../types/types').Types;

// Patch-up IE9
require('../util/legacy');

/*
 * GCLI is built from a number of components (called items) composed as
 * required for each environment.
 * When adding to or removing from this list, we should keep the basics in sync
 * with the other environments.
 * See:
 * - lib/gcli/index.js: Generic basic set (without commands)
 * - lib/gcli/demo.js: Adds demo commands to basic set for use in web demo
 * - gcli.js: Add commands to basic set for use in Node command line
 * - lib/gcli/index.js: (mozmaster branch) From scratch listing for Firefox
 * - lib/gcli/connectors/index.js: Client only items when executing remotely
 * - lib/gcli/connectors/direct.js: Test items for connecting to in-process GCLI
 */
var items = [
  // First we need to add the local types which other types depend on
  require('../types/delegate').items,
  require('../types/selection').items,
  require('../types/array').items,

  require('../types/boolean').items,
  require('../types/command').items,
  require('../types/date').items,
  require('../types/file').items,
  require('../types/javascript').items,
  require('../types/node').items,
  require('../types/number').items,
  require('../types/resource').items,
  require('../types/setting').items,
  require('../types/string').items,
  require('../types/union').items,
  require('../types/url').items,

  require('../fields/fields').items,
  require('../fields/delegate').items,
  require('../fields/selection').items,

  require('../ui/intro').items,
  require('../ui/focus').items,

  require('../converters/converters').items,
  require('../converters/basic').items,
  require('../converters/html').items,
  require('../converters/terminal').items,

  require('../languages/command').items,
  require('../languages/javascript').items,

  require('./direct').items,
  // require('./rdp').items, // Firefox remote debug protocol
  require('./websocket').items,
  require('./xhr').items,

  require('../commands/context').items,

].reduce(function(prev, curr) { return prev.concat(curr); }, []);

/**
 * These are the commands stored on the remote side that have converters which
 * we'll need to present the data
 */
var requiredConverters = [
  require('../cli').items,

  require('../commands/clear').items,
  require('../commands/connect').items,
  require('../commands/exec').items,
  require('../commands/global').items,
  require('../commands/help').items,
  require('../commands/intro').items,
  require('../commands/lang').items,
  require('../commands/preflist').items,
  require('../commands/pref').items,
  require('../commands/test').items,

].reduce(function(prev, curr) { return prev.concat(curr); }, [])
 .filter(function(item) { return item.item === 'converter'; });

/**
 * Connect to a remote system and setup the commands/types/converters etc needed
 * to make it all work
 */
exports.connect = function(options) {
  options = options || {};

  var system = api.createSystem();

  // Ugly hack, to aid testing
  exports.api = system;

  options.types = system.types = new Types();
  options.commands = system.commands = new Commands(system.types);

  system.addItems(items);
  system.addItems(requiredConverters);

  var connector = system.connectors.get(options.connector);
  return connector.connect(options.url).then(function(connection) {
    options.connection = connection;
    connection.on('commandsChanged', function(specs) {
      exports.addItems(system, specs, connection);
    });

    return connection.call('specs').then(function(specs) {
      exports.addItems(system, specs, connection);
      return connection;
    });
  });
};

exports.addItems = function(gcli, specs, connection) {
  exports.removeRemoteItems(gcli, connection);
  var remoteItems = exports.addLocalFunctions(specs, connection);
  gcli.addItems(remoteItems);
};

/**
 * Take the data from the 'specs' command (or the 'commandsChanged' event) and
 * add function to proxy the execution back over the connection
 */
exports.addLocalFunctions = function(specs, connection) {
  // Inject an 'exec' function into the commands, and the connection into
  // all the remote types
  specs.forEach(function(commandSpec) {
    //
    commandSpec.connection = connection;
    commandSpec.params.forEach(function(param) {
      param.type.connection = connection;
    });

    if (!commandSpec.isParent) {
      commandSpec.exec = function(args, context) {
        var data = {
          typed: (context.prefix ? context.prefix + ' ' : '') + context.typed
        };

        return connection.call('execute', data).then(function(reply) {
          var typedData = context.typedData(reply.type, reply.data);
          if (!reply.error) {
            return typedData;
          }
          else {
            throw typedData;
          }
        });
      };
    }

    commandSpec.isProxy = true;
  });

  return specs;
};

exports.removeRemoteItems = function(gcli, connection) {
  gcli.commands.getAll().forEach(function(command) {
    if (command.connection === connection) {
      gcli.commands.remove(command);
    }
  });
};
