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

var util = require('./util/util');
var Commands = require('./commands/commands').Commands;
var Connectors = require('./connectors/connectors').Connectors;
var Converters = require('./converters/converters').Converters;
var Fields = require('./fields/fields').Fields;
var Languages = require('./languages/languages').Languages;
var Settings = require('./settings').Settings;
var Types = require('./types/types').Types;

/**
 * This is the heart of the API that we expose to the outside.
 * @param options Object that customizes how the system acts. Valid properties:
 * - commands, connectors, converters, fields, languages, settings, types:
 *   Custom configured manager objects for these item types
 * - location: a system with a location will ignore commands that don't have a
 *   matching runAt property. This is principly for client/server setups where
 *   we import commands from the server to the client, so a system with
 *   `{ location: 'client' }` will silently ignore commands with
 *   `{ runAt: 'server' }`. Any system without a location will accept commands
 *   with any runAt property (including none).
 */
exports.createSystem = function(options) {
  options = options || {};
  var location = options.location;

  // The plural/singular thing may make you want to scream, but it allows us
  // to say components[getItemType(item)], so a lookup here (and below) saves
  // multiple lookups in the middle of the code
  var components = {
    connector: options.connectors || new Connectors(),
    converter: options.converters || new Converters(),
    field: options.fields || new Fields(),
    language: options.languages || new Languages(),
    type: options.types || new Types()
  };
  components.setting = new Settings(components.type);
  components.command = new Commands(components.type, location);

  var getItemType = function(item) {
    if (item.item) {
      return item.item;
    }
    // Some items are registered using the constructor so we need to check
    // the prototype for the the type of the item
    return (item.prototype && item.prototype.item) ?
           item.prototype.item : 'command';
  };

  var addItem = function(item) {
    try {
      components[getItemType(item)].add(item);
    }
    catch (ex) {
      if (item != null) {
        console.error('While adding: ' + item.name);
      }
      throw ex;
    }
  };

  var removeItem = function(item) {
    components[getItemType(item)].remove(item);
  };

  /**
   * loadableModules is a lookup of names to module loader functions (like
   * the venerable 'require') to which we can pass a name and get back a
   * JS object (or a promise of a JS object). This allows us to have custom
   * loaders to get stuff from the filesystem etc.
   */
  var loadableModules = {};

  /**
   * loadedModules is a lookup by name of the things returned by the functions
   * in loadableModules so we can track what we need to unload / reload.
   */
  var loadedModules = {};

  var unloadModule = function(name) {
    var existingModule = loadedModules[name];
    if (existingModule != null) {
      existingModule.items.forEach(removeItem);
    }
    delete loadedModules[name];
  };

  var loadModule = function(name) {
    var existingModule = loadedModules[name];
    unloadModule(name);

    // And load the new items
    try {
      var loader = loadableModules[name];
      return Promise.resolve(loader(name)).then(function(newModule) {
        if (existingModule === newModule) {
          return;
        }

        if (newModule == null) {
          throw 'Module \'' + name + '\' not found';
        }

        if (newModule.items == null || typeof newModule.items.forEach !== 'function') {
          console.log('Exported properties: ' + Object.keys(newModule).join(', '));
          throw 'Module \'' + name + '\' has no \'items\' array export';
        }

        newModule.items.forEach(addItem);

        loadedModules[name] = newModule;
      });
    }
    catch (ex) {
      console.error('Failed to load module ' + name + ': ' + ex);
      console.error(ex.stack);

      return Promise.resolve();
    }
  };

  var pendingChanges = false;

  var system = {
    addItems: function(items) {
      items.forEach(addItem);
    },

    removeItems: function(items) {
      items.forEach(removeItem);
    },

    addItemsByModule: function(names, options) {
      var promises = [];

      options = options || {};
      if (!options.delayedLoad) {
        // We could be about to add many commands, just report the change once
        this.commands.onCommandsChange.holdFire();
      }

      if (typeof names === 'string') {
        names = [ names ];
      }
      names.forEach(function(name) {
        if (options.loader == null) {
          options.loader = function(name) {
            return require(name);
          };
        }
        loadableModules[name] = options.loader;

        if (options.delayedLoad) {
          pendingChanges = true;
        }
        else {
          promises.push(loadModule(name).catch(console.error));
        }
      });

      if (options.delayedLoad) {
        return Promise.resolve();
      }
      else {
        return Promise.all(promises).then(function() {
          this.commands.onCommandsChange.resumeFire();
        }.bind(this));
      }
    },

    removeItemsByModule: function(name) {
      this.commands.onCommandsChange.holdFire();

      delete loadableModules[name];
      unloadModule(name);

      this.commands.onCommandsChange.resumeFire();
    },

    load: function() {
      if (!pendingChanges) {
        return Promise.resolve();
      }
      this.commands.onCommandsChange.holdFire();

      // clone loadedModules, so we can remove what is left at the end
      var modules = Object.keys(loadedModules).map(function(name) {
        return loadedModules[name];
      });

      var promises = Object.keys(loadableModules).map(function(name) {
        delete modules[name];
        return loadModule(name).catch(console.error);
      });

      Object.keys(modules).forEach(unloadModule);
      pendingChanges = false;

      return Promise.all(promises).then(function() {
        this.commands.onCommandsChange.resumeFire();
      }.bind(this));
    },

    destroy: function() {
      this.commands.onCommandsChange.holdFire();

      Object.keys(loadedModules).forEach(function(name) {
        unloadModule(name);
      });

      this.commands.onCommandsChange.resumeFire();
    },

    toString: function() {
      return 'System [' +
             'commands:' + components.command.getAll().length + ', ' +
             'connectors:' + components.connector.getAll().length + ', ' +
             'converters:' + components.converter.getAll().length + ', ' +
             'fields:' + components.field.getAll().length + ', ' +
             'settings:' + components.setting.getAll().length + ', ' +
             'types:' + components.type.getTypeNames().length + ']';
    }
  };

  Object.defineProperty(system, 'commands', {
    get: function() { return components.command; },
    enumerable: true
  });

  Object.defineProperty(system, 'connectors', {
    get: function() { return components.connector; },
    enumerable: true
  });

  Object.defineProperty(system, 'converters', {
    get: function() { return components.converter; },
    enumerable: true
  });

  Object.defineProperty(system, 'fields', {
    get: function() { return components.field; },
    enumerable: true
  });

  Object.defineProperty(system, 'languages', {
    get: function() { return components.language; },
    enumerable: true
  });

  Object.defineProperty(system, 'settings', {
    get: function() { return components.setting; },
    enumerable: true
  });

  Object.defineProperty(system, 'types', {
    get: function() { return components.type; },
    enumerable: true
  });

  return system;
};

/**
 * Connect a local system with another at the other end of a connector
 * @param system System to which we're adding commands
 * @param front Front which allows access to the remote system from which we
 * import commands
 * @param customProps Array of strings specifying additional properties defined
 * on remote commands that should be considered part of the metadata for the
 * commands imported into the local system
 */
exports.connectFront = function(system, front, customProps) {
  system._handleCommandsChanged = function() {
    syncItems(system, front, customProps).catch(util.errorHandler);
  };
  front.on('commands-changed', system._handleCommandsChanged);

  return syncItems(system, front, customProps);
};

/**
 * Undo the effect of #connectFront
 */
exports.disconnectFront = function(system, front) {
  front.off('commands-changed', system._handleCommandsChanged);
  system._handleCommandsChanged = undefined;
  removeItemsFromFront(system, front);
};

/**
 * Remove the items in this system that came from a previous sync action, and
 * re-add them. See connectFront() for explanation of properties
 */
function syncItems(system, front, customProps) {
  return front.specs(customProps).then(function(specs) {
    removeItemsFromFront(system, front);

    var remoteItems = addLocalFunctions(specs, front);
    system.addItems(remoteItems);

    return system;
  });
};

/**
 * Take the data from the 'specs' command (or the 'commands-changed' event) and
 * add function to proxy the execution back over the front
 */
function addLocalFunctions(specs, front) {
  // Inject an 'exec' function into the commands, and the front into
  // all the remote types
  specs.forEach(function(commandSpec) {
    // HACK: Tack the front to the command so we know how to remove it
    // in removeItemsFromFront() below
    commandSpec.front = front;

    // Tell the type instances for a command how to contact their counterparts
    // Don't confuse this with setting the front on the commandSpec which is
    // about associating a proxied command with it's source for later removal.
    // This is actually going to be used by the type
    commandSpec.params.forEach(function(param) {
      if (typeof param.type !== 'string') {
        param.type.front = front;
      }
    });

    if (!commandSpec.isParent) {
      commandSpec.exec = function(args, context) {
        var typed = (context.prefix ? context.prefix + ' ' : '') + context.typed;
        return front.execute(typed).then(function(reply) {
          var typedData = context.typedData(reply.type, reply.data);
          return reply.isError ? Promise.reject(typedData) : typedData;
        });
      };
    }

    commandSpec.isProxy = true;
  });

  return specs;
}

/**
 * Go through all the commands removing any that are associated with the
 * given front. The method of association is the hack in addLocalFunctions.
 */
function removeItemsFromFront(system, front) {
  system.commands.getAll().forEach(function(command) {
    if (command.front === front) {
      system.commands.remove(command);
    }
  });
}
