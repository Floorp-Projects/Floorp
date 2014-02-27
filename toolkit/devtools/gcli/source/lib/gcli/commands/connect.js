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

var l10n = require('../util/l10n');
var connectors = require('../connectors/connectors');
var cli = require('../cli');

/**
 * A lookup of the current connection
 */
var connections = {};

/**
 * 'connection' type
 */
var connection = {
  item: 'type',
  name: 'connection',
  parent: 'selection',
  lookup: function() {
    return Object.keys(connections).map(function(prefix) {
      return { name: prefix, value: connections[prefix] };
    });
  }
};

/**
 * 'connector' type
 */
var connector = {
  item: 'type',
  name: 'connector',
  parent: 'selection',
  lookup: function() {
    return connectors.getConnectors().map(function(connector) {
      return { name: connector.name, value: connector };
    });
  }
};

/**
 * 'connect' command
 */
var connect = {
  item: 'command',
  name: 'connect',
  description: l10n.lookup('connectDesc'),
  manual: l10n.lookup('connectManual'),
  params: [
    {
      name: 'prefix',
      type: 'string',
      description: l10n.lookup('connectPrefixDesc')
    },
    {
      name: 'method',
      short: 'm',
      type: 'connector',
      description: l10n.lookup('connectMethodDesc'),
      defaultValue: undefined, // set to connectors.get('xhr') below
      option: true
    },
    {
      name: 'url',
      short: 'u',
      type: 'string',
      description: l10n.lookup('connectUrlDesc'),
      defaultValue: null,
      option: true
    }
  ],
  returnType: 'string',

  exec: function(args, context) {
    if (connections[args.prefix] != null) {
      throw new Error(l10n.lookupFormat('connectDupReply', [ args.prefix ]));
    }

    return args.method.connect(args.url).then(function(connection) {
      // Nasty: stash the prefix on the connection to help us tidy up
      connection.prefix = args.prefix;
      connections[args.prefix] = connection;

      return connection.call('specs').then(function(specs) {
        var remoter = this.createRemoter(args.prefix, connection);
        var canon = cli.getMapping(context).requisition.canon;
        canon.addProxyCommands(specs, remoter, args.prefix, args.url);

        // TODO: We should add type proxies here too

        // commandSpecs doesn't include the parent command that we added
        return l10n.lookupFormat('connectReply',
                                 [ Object.keys(specs).length + 1 ]);
      }.bind(this));
    }.bind(this));
  },

  /**
   * When we register a set of remote commands, we need to provide the canon
   * with a proxy executor. This is that executor.
   */
  createRemoter: function(prefix, connection) {
    return function(cmdArgs, context) {
      var typed = context.typed;

      // If we've been called using a 'context' then there will be no prefix
      // otherwise we need to remove it
      if (typed.indexOf(prefix) === 0) {
        typed = typed.substring(prefix.length).replace(/^ */, '');
      }

      var data = {
        typed: typed,
        args: cmdArgs
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
    }.bind(this);
  }
};

/**
 * We just need to call connectors.get later than module load time to
 * enable something to load the xhr module
 */
Object.defineProperty(connect.params[1], 'defaultValue', {
  get: function() {
    return connectors.get('xhr');
  },
  enumerable : true
});

/**
 * 'disconnect' command
 */
var disconnect = {
  item: 'command',
  name: 'disconnect',
  description: l10n.lookup('disconnectDesc2'),
  manual: l10n.lookup('disconnectManual2'),
  params: [
    {
      name: 'prefix',
      type: 'connection',
      description: l10n.lookup('disconnectPrefixDesc')
    }
  ],
  returnType: 'string',

  exec: function(args, context) {
    var connection = args.prefix;
    return connection.disconnect().then(function() {
      var canon = cli.getMapping(context).requisition.canon;
      var removed = canon.removeProxyCommands(connection.prefix);
      delete connections[connection.prefix];
      return l10n.lookupFormat('disconnectReply', [ removed.length ]);
    });
  }
};

exports.items = [ connection, connector, connect, disconnect ];
