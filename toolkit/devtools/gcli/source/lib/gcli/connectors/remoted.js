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

/* jshint quotmark:false, newcap:false */

'use strict';

var Promise = require('../util/promise').Promise;
var host = require('../util/host');
var fileparser = require('../util/fileparser');

var protocol = require('./protocol');
var method = protocol.method;
var Arg = protocol.Arg;
var RetVal = protocol.RetVal;

/**
 * Provide JSON mapping services to remote functionality of a Requisition
 */
var Remoter = exports.Remoter = function(requisition) {
  this.requisition = requisition;
  this._listeners = [];
};

/**
 * Add a new listener
 */
Remoter.prototype.addListener = function(action) {
  var listener = {
    action: action,
    caller: function() {
      action('canonChanged', this.requisition.canon.getCommandSpecs());
    }.bind(this)
  };
  this._listeners.push(listener);

  this.requisition.canon.onCanonChange.add(listener.caller);
};

/**
 * Remove an existing listener
 */
Remoter.prototype.removeListener = function(action) {
  var listener;

  this._listeners = this._listeners.filter(function(li) {
    if (li.action === action) {
      listener = li;
      return false;
    }
    return true;
  });

  if (listener == null) {
    throw new Error('action not a known listener');
  }

  this.requisition.canon.onCanonChange.remove(listener.caller);
};

/**
 * These functions are designed to be remoted via RDP/XHR/websocket, etc
 */
Remoter.prototype.exposed = {
  /**
   * Retrieve a list of the remotely executable commands
   */
  specs: method(function() {
    return this.requisition.canon.getCommandSpecs();
  }, {
    request: {},
    response: RetVal("json")
  }),

  /**
   * Execute a GCLI command
   * @return a promise of an object with the following properties:
   * - data: The output of the command
   * - type: The type of the data to allow selection of a converter
   * - error: True if the output was considered an error
   */
  execute: method(function(typed) {
    return this.requisition.updateExec(typed).then(function(output) {
      return output.toJson();
    });
  }, {
    request: {
      typed: Arg(0, "string") // The command string
    },
    response: RetVal("json")
  }),

  /**
   * Get the state of an input string. i.e. requisition.getStateData()
   */
  state: method(function(typed, start, rank) {
    return this.requisition.update(typed).then(function() {
      return this.requisition.getStateData(start, rank);
    }.bind(this));
  }, {
    request: {
      typed: Arg(0, "string"), // The command string
      start: Arg(1, "number"), // Cursor start position
      rank: Arg(2, "number") // The prediction offset (# times UP/DOWN pressed)
    },
    response: RetVal("json")
  }),

  /**
   * Call type.parse to check validity. Used by the remote type
   * @return a promise of an object with the following properties:
   * - status: Of of the following strings: VALID|INCOMPLETE|ERROR
   * - message: The message to display to the user
   * - predictions: An array of suggested values for the given parameter
   */
  typeparse: method(function(typed, param) {
    return this.requisition.update(typed).then(function() {
      var assignment = this.requisition.getAssignment(param);

      return Promise.resolve(assignment.predictions).then(function(predictions) {
        return {
          status: assignment.getStatus().toString(),
          message: assignment.message,
          predictions: predictions
        };
      });
    }.bind(this));
  }, {
    request: {
      typed: Arg(0, "string"), // The command string
      param: Arg(1, "string") // The name of the parameter to parse
    },
    response: RetVal("json")
  }),

  /**
   * Get the incremented value of some type
   * @return a promise of a string containing the new argument text
   */
  typeincrement: method(function(typed, param) {
    return this.requisition.update(typed).then(function() {
      var assignment = this.requisition.getAssignment(param);
      return this.requisition.increment(assignment).then(function() {
        var arg = assignment.arg;
        return arg == null ? undefined : arg.text;
      });
    });
  }, {
    request: {
      typed: Arg(0, "string"), // The command string
      param: Arg(1, "string") // The name of the parameter to parse
    },
    response: RetVal("string")
  }),

  /**
   * See typeincrement
   */
  typedecrement: method(function(typed, param) {
    return this.requisition.update(typed).then(function() {
      var assignment = this.requisition.getAssignment(param);
      return this.requisition.decrement(assignment).then(function() {
        var arg = assignment.arg;
        return arg == null ? undefined : arg.text;
      });
    });
  }, {
    request: {
      typed: Arg(0, "string"), // The command string
      param: Arg(1, "string") // The name of the parameter to parse
    },
    response: RetVal("string")
  }),

  /**
   * Perform a lookup on a selection type to get the allowed values
   */
  selectioninfo: method(function(commandName, paramName, action) {
    var command = this.requisition.canon.getCommand(commandName);
    if (command == null) {
      throw new Error('No command called \'' + commandName + '\'');
    }

    var type;
    command.params.forEach(function(param) {
      if (param.name === paramName) {
        type = param.type;
      }
    });
    if (type == null) {
      throw new Error('No parameter called \'' + paramName + '\' in \'' +
                      commandName + '\'');
    }

    switch (action) {
      case 'lookup':
        return type.lookup(this.requisition.executionContext);
      case 'data':
        return type.data(this.requisition.executionContext);
      default:
        throw new Error('Action must be either \'lookup\' or \'data\'');
    }
  }, {
    request: {
      commandName: Arg(0, "string"), // The command containing the parameter in question
      paramName: Arg(1, "string"), // The name of the parameter
      action: Arg(2, "string") // 'lookup' or 'data' depending on the function to call
    },
    response: RetVal("json")
  }),

  /**
   * Execute a system command
   * @return a promise of a string containing the output of the command
   */
  system: method(function(cmd, args, cwd, env) {
    return host.spawn({ cmd: cmd, args: args, cwd: cwd, env: env });
  }, {
    request: {
      cmd: Arg(0, "string"), // The executable to call
      args: Arg(1, "array:string"), // Arguments to the executable
      cwd: Arg(2, "string"), // The working directory
      env: Arg(3, "json") // A map of environment variables
    },
    response: RetVal("json")
  }),

  /**
   * Examine the filesystem for file matches
   */
  parsefile: method(function(typed, filetype, existing, matches) {
    var options = {
      filetype: filetype,
      existing: existing,
      matches: new RegExp(matches)
    };

    return fileparser.parse(typed, options).then(function(reply) {
      reply.status = reply.status.toString();
      if (reply.predictor == null) {
        return reply;
      }

      return reply.predictor().then(function(predictions) {
        delete reply.predictor;
        reply.predictions = predictions;
        return reply;
      });
    });
  }, {
    request: {
      typed: Arg(0, "string"), // The filename as typed by the user
      filetype: Arg(1, "array:string"), // The expected filetype
      existing: Arg(2, "string"), // Boolean which defines if a file/directory is expected to exist
      matches: Arg(3, "json") // String of a regular expression which the result should match
    },
    response: RetVal("json")
  })
};
