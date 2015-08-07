/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Task } = require("resource://gre/modules/Task.jsm");
const {
  method, Arg, Option, RetVal, Front, FrontClass, Actor, ActorClass
} = require("devtools/server/protocol");
const events = require("sdk/event/core");
const { createSystem } = require("gcli/system");

/**
 * Manage remote connections that want to talk to GCLI
 */
const GcliActor = ActorClass({
  typeName: "gcli",

  events: {
    "commands-changed" : {
      type: "commandsChanged"
    }
  },

  initialize: function(conn, tabActor) {
    Actor.prototype.initialize.call(this, conn);

    this._commandsChanged = this._commandsChanged.bind(this);

    this._tabActor = tabActor;
    this._requisitionPromise = undefined; // see _getRequisition()
  },

  disconnect: function() {
    return this.destroy();
  },

  destroy: function() {
    Actor.prototype.destroy.call(this);

    // If _getRequisition has not been called, just bail quickly
    if (this._requisitionPromise == null) {
      this._commandsChanged = undefined;
      this._tabActor = undefined;
      return Promise.resolve();
    }

    return this._getRequisition().then(requisition => {
      requisition.destroy();

      this._system.commands.onCommandsChange.remove(this._commandsChanged);
      this._system.destroy();
      this._system = undefined;

      this._requisitionPromise = undefined;
      this._tabActor = undefined;

      this._commandsChanged = undefined;
    });
  },

  /**
   * Load a module into the requisition
   */
  _testOnly_addItemsByModule: method(function(names) {
    return this._getRequisition().then(requisition => {
      return requisition.system.addItemsByModule(names);
    });
  }, {
    request: {
      customProps: Arg(0, "array:string")
    }
  }),

  /**
   * Unload a module from the requisition
   */
  _testOnly_removeItemsByModule: method(function(names) {
    return this._getRequisition().then(requisition => {
      return requisition.system.removeItemsByModule(names);
    });
  }, {
    request: {
      customProps: Arg(0, "array:string")
    }
  }),

  /**
   * Retrieve a list of the remotely executable commands
   * @param customProps Array of strings containing additional properties which,
   * if specified in the command spec, will be included in the JSON. Normally we
   * transfer only the properties required for GCLI to function.
   */
  specs: method(function(customProps) {
    return this._getRequisition().then(requisition => {
      return requisition.system.commands.getCommandSpecs(customProps);
    });
  }, {
    request: {
      customProps: Arg(0, "nullable:array:string")
    },
    response: {
      value: RetVal("array:json")
    }
  }),

  /**
   * Execute a GCLI command
   * @return a promise of an object with the following properties:
   * - data: The output of the command
   * - type: The type of the data to allow selection of a converter
   * - error: True if the output was considered an error
   */
  execute: method(function(typed) {
    return this._getRequisition().then(requisition => {
      return requisition.updateExec(typed).then(output => output.toJson());
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
    return this._getRequisition().then(requisition => {
      return requisition.update(typed).then(() => {
        return requisition.getStateData(start, rank);
      });
    });
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
  parseType: method(function(typed, paramName) {
    return this._getRequisition().then(requisition => {
      return requisition.update(typed).then(() => {
        let assignment = requisition.getAssignment(paramName);
        return Promise.resolve(assignment.predictions).then(predictions => {
          return {
            status: assignment.getStatus().toString(),
            message: assignment.message,
            predictions: predictions
          };
        });
      });
    });
  }, {
    request: {
      typed: Arg(0, "string"), // The command string
      paramName: Arg(1, "string") // The name of the parameter to parse
    },
    response: RetVal("json")
  }),

  /**
   * Get the incremented/decremented value of some type
   * @return a promise of a string containing the new argument text
   */
  nudgeType: method(function(typed, by, paramName) {
    return this.requisition.update(typed).then(() => {
      const assignment = this.requisition.getAssignment(paramName);
      return this.requisition.nudge(assignment, by).then(() => {
        return assignment.arg == null ? undefined : assignment.arg.text;
      });
    });
  }, {
    request: {
      typed: Arg(0, "string"),    // The command string
      by: Arg(1, "number"),       // +1/-1 for increment / decrement
      paramName: Arg(2, "string") // The name of the parameter to parse
    },
    response: RetVal("string")
  }),

  /**
   * Perform a lookup on a selection type to get the allowed values
   */
  getSelectionLookup: method(function(commandName, paramName) {
    return this._getRequisition().then(requisition => {
      const command = requisition.system.commands.get(commandName);
      if (command == null) {
        throw new Error("No command called '" + commandName + "'");
      }

      let type;
      command.params.forEach(param => {
        if (param.name === paramName) {
          type = param.type;
        }
      });

      if (type == null) {
        throw new Error("No parameter called '" + paramName + "' in '" +
                        commandName + "'");
      }

      const reply = type.getLookup(requisition.executionContext);
      return Promise.resolve(reply).then(lookup => {
        // lookup returns an array of objects with name/value properties and
        // the values might not be JSONable, so remove them
        return lookup.map(info => ({ name: info.name }));
      });
    });
  }, {
    request: {
      commandName: Arg(0, "string"), // The command containing the parameter in question
      paramName: Arg(1, "string"),   // The name of the parameter
    },
    response: RetVal("json")
  }),

  /**
   * Lazy init for a Requisition
   */
  _getRequisition: function() {
    if (this._tabActor == null) {
      throw new Error('GcliActor used post-destroy');
    }

    if (this._requisitionPromise != null) {
      return this._requisitionPromise;
    }

    const Requisition = require("gcli/cli").Requisition;
    const tabActor = this._tabActor;

    this._system = createSystem({ location: "server" });
    this._system.commands.onCommandsChange.add(this._commandsChanged);

    const gcliInit = require("devtools/toolkit/gcli/commands/index");
    gcliInit.addAllItemsByModule(this._system);

    // this._requisitionPromise should be created synchronously with the call
    // to _getRequisition so that destroy can tell whether there is an async
    // init in progress
    this._requisitionPromise = this._system.load().then(() => {
      const environment = {
        get chromeWindow() {
          throw new Error("environment.chromeWindow is not available in runAt:server commands");
        },

        get chromeDocument() {
          throw new Error("environment.chromeDocument is not available in runAt:server commands");
        },

        get window() {
          return tabActor.window;
        },

        get document() {
          return tabActor.window && tabActor.window.document;
        }
      };

      return new Requisition(this._system, { environment: environment });
    });

    return this._requisitionPromise;
  },

  /**
   * Pass events from requisition.system.commands.onCommandsChange upwards
   */
  _commandsChanged: function() {
    events.emit(this, "commands-changed");
  },
});

exports.GcliActor = GcliActor;

/**
 *
 */
const GcliFront = exports.GcliFront = FrontClass(GcliActor, {
  initialize: function(client, tabForm) {
    Front.prototype.initialize.call(this, client);
    this.actorID = tabForm.gcliActor;

    // XXX: This is the first actor type in its hierarchy to use the protocol
    // library, so we're going to self-own on the client side for now.
    this.manage(this);
  },
});

// A cache of created fronts: WeakMap<Client, Front>
const knownFronts = new WeakMap();

/**
 * Create a GcliFront only when needed (returns a promise)
 * For notes on target.makeRemote(), see
 * https://bugzilla.mozilla.org/show_bug.cgi?id=1016330#c7
 */
exports.GcliFront.create = function(target) {
  return target.makeRemote().then(() => {
    let front = knownFronts.get(target.client);
    if (front == null && target.form.gcliActor != null) {
      front = new GcliFront(target.client, target.form);
      knownFronts.set(target.client, front);
    }
    return front;
  });
};
