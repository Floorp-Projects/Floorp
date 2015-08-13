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
var spell = require('../util/spell');
var SelectionType = require('./selection').SelectionType;
var Status = require('./types').Status;
var Conversion = require('./types').Conversion;
var cli = require('../cli');

exports.items = [
  {
    // Select from the available parameters to a command
    item: 'type',
    name: 'param',
    parent: 'selection',
    stringifyProperty: 'name',
    requisition: undefined,
    isIncompleteName: undefined,

    getSpec: function() {
      throw new Error('param type is not remotable');
    },

    lookup: function() {
      return exports.getDisplayedParamLookup(this.requisition);
    },

    parse: function(arg, context) {
      if (this.isIncompleteName) {
        return SelectionType.prototype.parse.call(this, arg, context);
      }
      else {
        var message = l10n.lookup('cliUnusedArg');
        return Promise.resolve(new Conversion(undefined, arg, Status.ERROR, message));
      }
    }
  },
  {
    // Select from the available commands
    // This is very similar to a SelectionType, however the level of hackery in
    // SelectionType to make it handle Commands correctly was to high, so we
    // simplified.
    // If you are making changes to this code, you should check there too.
    item: 'type',
    name: 'command',
    parent: 'selection',
    stringifyProperty: 'name',
    allowNonExec: true,

    getSpec: function() {
      return {
        name: 'command',
        allowNonExec: this.allowNonExec
      };
    },

    lookup: function(context) {
      var commands = cli.getMapping(context).requisition.system.commands;
      return exports.getCommandLookup(commands);
    },

    parse: function(arg, context) {
      var conversion = exports.parse(context, arg, this.allowNonExec);
      return Promise.resolve(conversion);
    }
  }
];

exports.getDisplayedParamLookup = function(requisition) {
  var displayedParams = [];
  var command = requisition.commandAssignment.value;
  if (command != null) {
    command.params.forEach(function(param) {
      var arg = requisition.getAssignment(param.name).arg;
      if (!param.isPositionalAllowed && arg.type === 'BlankArgument') {
        displayedParams.push({ name: '--' + param.name, value: param });
      }
    });
  }
  return displayedParams;
};

exports.parse = function(context, arg, allowNonExec) {
  var commands = cli.getMapping(context).requisition.system.commands;
  var lookup = exports.getCommandLookup(commands);
  var predictions = exports.findPredictions(arg, lookup);
  return exports.convertPredictions(commands, arg, allowNonExec, predictions);
};

exports.getCommandLookup = function(commands) {
  var sorted = commands.getAll().sort(function(c1, c2) {
    return c1.name.localeCompare(c2.name);
  });
  return sorted.map(function(command) {
    return { name: command.name, value: command };
  });
};

exports.findPredictions = function(arg, lookup) {
  var predictions = [];
  var i, option;
  var maxPredictions = Conversion.maxPredictions;
  var match = arg.text.toLowerCase();

  // Add an option to our list of predicted options
  var addToPredictions = function(option) {
    if (arg.text.length === 0) {
      // If someone hasn't typed anything, we only show top level commands in
      // the menu. i.e. sub-commands (those with a space in their name) are
      // excluded. We do this to keep the list at an overview level.
      if (option.name.indexOf(' ') === -1) {
        predictions.push(option);
      }
    }
    else {
      // If someone has typed something, then we exclude parent commands
      // (those without an exec). We do this because the user is drilling
      // down and doesn't need the summary level.
      if (option.value.exec != null) {
        predictions.push(option);
      }
    }
  };

  // If the arg has a suffix then we're kind of 'done'. Only an exact
  // match will do.
  if (arg.suffix.match(/ +/)) {
    for (i = 0; i < lookup.length && predictions.length < maxPredictions; i++) {
      option = lookup[i];
      if (option.name === arg.text ||
          option.name.indexOf(arg.text + ' ') === 0) {
        addToPredictions(option);
      }
    }

    return predictions;
  }

  // Cache lower case versions of all the option names
  for (i = 0; i < lookup.length; i++) {
    option = lookup[i];
    if (option._gcliLowerName == null) {
      option._gcliLowerName = option.name.toLowerCase();
    }
  }

  // Exact hidden matches. If 'hidden: true' then we only allow exact matches
  // All the tests after here check that !option.value.hidden
  for (i = 0; i < lookup.length && predictions.length < maxPredictions; i++) {
    option = lookup[i];
    if (option.name === arg.text) {
      addToPredictions(option);
    }
  }

  // Start with prefix matching
  for (i = 0; i < lookup.length && predictions.length < maxPredictions; i++) {
    option = lookup[i];
    if (option._gcliLowerName.indexOf(match) === 0 && !option.value.hidden) {
      if (predictions.indexOf(option) === -1) {
        addToPredictions(option);
      }
    }
  }

  // Try infix matching if we get less half max matched
  if (predictions.length < (maxPredictions / 2)) {
    for (i = 0; i < lookup.length && predictions.length < maxPredictions; i++) {
      option = lookup[i];
      if (option._gcliLowerName.indexOf(match) !== -1 && !option.value.hidden) {
        if (predictions.indexOf(option) === -1) {
          addToPredictions(option);
        }
      }
    }
  }

  // Try fuzzy matching if we don't get a prefix match
  if (predictions.length === 0) {
    var names = [];
    lookup.forEach(function(opt) {
      if (!opt.value.hidden) {
        names.push(opt.name);
      }
    });
    var corrected = spell.correct(match, names);
    if (corrected) {
      lookup.forEach(function(opt) {
        if (opt.name === corrected) {
          predictions.push(opt);
        }
      });
    }
  }

  return predictions;
};

exports.convertPredictions = function(commands, arg, allowNonExec, predictions) {
  var command = commands.get(arg.text);
  // Helper function - Commands like 'context' work best with parent
  // commands which are not executable. However obviously to execute a
  // command, it needs an exec function.
  var execWhereNeeded = (allowNonExec ||
                  (command != null && typeof command.exec === 'function'));

  var isExact = command && command.name === arg.text &&
                execWhereNeeded && predictions.length === 1;
  var alternatives = isExact ? [] : predictions;

  if (command) {
    var status = execWhereNeeded ? Status.VALID : Status.INCOMPLETE;
    return new Conversion(command, arg, status, '', alternatives);
  }

  if (predictions.length === 0) {
    var msg = l10n.lookupFormat('typesSelectionNomatch', [ arg.text ]);
    return new Conversion(undefined, arg, Status.ERROR, msg, alternatives);
  }

  command = predictions[0].value;

  if (predictions.length === 1) {
    // Is it an exact match of an executable command,
    // or just the only possibility?
    if (command.name === arg.text && execWhereNeeded) {
      return new Conversion(command, arg, Status.VALID, '');
    }

    return new Conversion(undefined, arg, Status.INCOMPLETE, '', alternatives);
  }

  // It's valid if the text matches, even if there are several options
  if (predictions[0].name === arg.text) {
    return new Conversion(command, arg, Status.VALID, '', alternatives);
  }

  return new Conversion(undefined, arg, Status.INCOMPLETE, '', alternatives);
};
