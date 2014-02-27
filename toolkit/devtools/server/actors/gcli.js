/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Cu = require('chrome').Cu;
var XPCOMUtils = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {}).XPCOMUtils;

XPCOMUtils.defineLazyModuleGetter(this, "console",
                                  "resource://gre/modules/devtools/Console.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CommandUtils",
                                  "resource:///modules/devtools/DeveloperToolbar.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "Requisition", function() {
  return require("gcli/cli").Requisition;
});

XPCOMUtils.defineLazyGetter(this, "centralCanon", function() {
  return require("gcli/commands/commands").centralCanon;
});

var util = require('gcli/util/util');

var protocol = require("devtools/server/protocol");
var method = protocol.method;
var Arg = protocol.Arg;
var Option = protocol.Option;
var RetVal = protocol.RetVal;

/**
 * Manage remote connections that want to talk to GCLI
 */
var GcliActor = protocol.ActorClass({
  typeName: "gcli",

  initialize: function(conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.tabActor = tabActor;
    let browser = tabActor.browser;

    let environment = {
      chromeWindow: browser.ownerGlobal,
      chromeDocument: browser.ownerDocument,
      window: browser.contentWindow,
      document: browser.contentDocument
    };

    this.requisition = new Requisition({ environment: env });
  },

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

      return promise.resolve(assignment.predictions).then(function(predictions) {
        return {
          status: assignment.getStatus().toString(),
          message: assignment.message,
          predictions: predictions
        };
      });
    });
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
        return assignment.arg == null ? undefined : assignment.arg.text;
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
        return assignment.arg == null ? undefined : assignment.arg.text;
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
        return type.lookup(context);
      case 'data':
        return type.data(context);
      default:
        throw new Error('Action must be either \'lookup\' or \'data\'');
    }
  }, {
    request: {
      typed: Arg(0, "string"), // The command containing the parameter in question
      param: Arg(1, "string"), // The name of the parameter
      action: Arg(1, "string") // 'lookup' or 'data' depending on the function to call
    },
    response: RetVal("json")
  })
});

exports.GcliFront = protocol.FrontClass(GcliActor, {
  initialize: function(client, tabForm) {
    protocol.Front.prototype.initialize.call(this, client);
    this.actorID = tabForm.gcliActor;

    // XXX: This is the first actor type in its hierarchy to use the protocol
    // library, so we're going to self-own on the client side for now.
    client.addActorPool(this);
    this.manage(this);
  },
});

/**
 * Called the framework on DebuggerServer.registerModule()
 */
exports.register = function(handle) {
  handle.addTabActor(GcliActor, "gcliActor");
};

exports.unregister = function(handle) {
  handle.removeTabActor(GcliActor);
};
