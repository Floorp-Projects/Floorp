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

var util = require('../util/util');
var Promise = require('../util/promise').Promise;

/**
 * We record where in the input string an argument comes so we can report
 * errors against those string positions.
 * @param text The string (trimmed) that contains the argument
 * @param prefix Knowledge of quotation marks and whitespace used prior to the
 * text in the input string allows us to re-generate the original input from
 * the arguments.
 * @param suffix Any quotation marks and whitespace used after the text.
 * Whitespace is normally placed in the prefix to the succeeding argument, but
 * can be used here when this is the last argument.
 * @constructor
 */
function Argument(text, prefix, suffix) {
  if (text === undefined) {
    this.text = '';
    this.prefix = '';
    this.suffix = '';
  }
  else {
    this.text = text;
    this.prefix = prefix !== undefined ? prefix : '';
    this.suffix = suffix !== undefined ? suffix : '';
  }
}

Argument.prototype.type = 'Argument';

/**
 * Return the result of merging these arguments.
 * case and some of the arguments are in quotation marks?
 */
Argument.prototype.merge = function(following) {
  // Is it possible that this gets called when we're merging arguments
  // for the single string?
  return new Argument(
    this.text + this.suffix + following.prefix + following.text,
    this.prefix, following.suffix);
};

/**
 * Returns a new Argument like this one but with various items changed.
 * @param options Values to use in creating a new Argument.
 * Warning: some implementations of beget make additions to the options
 * argument. You should be aware of this in the unlikely event that you want to
 * reuse 'options' arguments.
 * Properties:
 * - text: The new text value
 * - prefixSpace: Should the prefix be altered to begin with a space?
 * - prefixPostSpace: Should the prefix be altered to end with a space?
 * - suffixSpace: Should the suffix be altered to end with a space?
 * - type: Constructor to use in creating new instances. Default: Argument
 * - dontQuote: Should we avoid adding prefix/suffix quotes when the text value
 *   has a space? Needed when we're completing a sub-command.
 */
Argument.prototype.beget = function(options) {
  var text = this.text;
  var prefix = this.prefix;
  var suffix = this.suffix;

  if (options.text != null) {
    text = options.text;

    // We need to add quotes when the replacement string has spaces or is empty
    if (!options.dontQuote) {
      var needsQuote = text.indexOf(' ') >= 0 || text.length === 0;
      var hasQuote = /['"]$/.test(prefix);
      if (needsQuote && !hasQuote) {
        prefix = prefix + '\'';
        suffix = '\'' + suffix;
      }
    }
  }

  if (options.prefixSpace && prefix.charAt(0) !== ' ') {
    prefix = ' ' + prefix;
  }

  if (options.prefixPostSpace && prefix.charAt(prefix.length - 1) !== ' ') {
    prefix = prefix + ' ';
  }

  if (options.suffixSpace && suffix.charAt(suffix.length - 1) !== ' ') {
    suffix = suffix + ' ';
  }

  if (text === this.text && suffix === this.suffix && prefix === this.prefix) {
    return this;
  }

  var ArgumentType = options.type || Argument;
  return new ArgumentType(text, prefix, suffix);
};

/**
 * We need to keep track of which assignment we've been assigned to
 */
Object.defineProperty(Argument.prototype, 'assignment', {
  get: function() { return this._assignment; },
  set: function(assignment) { this._assignment = assignment; },
  enumerable: true
});

/**
 * Sub-classes of Argument are collections of arguments, getArgs() gets access
 * to the members of the collection in order to do things like re-create input
 * command lines. For the simple Argument case it's just an array containing
 * only this.
 */
Argument.prototype.getArgs = function() {
  return [ this ];
};

/**
 * We define equals to mean all arg properties are strict equals.
 * Used by Conversion.argEquals and Conversion.equals and ultimately
 * Assignment.equals to avoid reporting a change event when a new conversion
 * is assigned.
 */
Argument.prototype.equals = function(that) {
  if (this === that) {
    return true;
  }
  if (that == null || !(that instanceof Argument)) {
    return false;
  }

  return this.text === that.text &&
       this.prefix === that.prefix && this.suffix === that.suffix;
};

/**
 * Helper when we're putting arguments back together
 */
Argument.prototype.toString = function() {
  // BUG 664207: We should re-escape escaped characters
  // But can we do that reliably?
  return this.prefix + this.text + this.suffix;
};

/**
 * Merge an array of arguments into a single argument.
 * All Arguments in the array are expected to have the same emitter
 */
Argument.merge = function(argArray, start, end) {
  start = (start === undefined) ? 0 : start;
  end = (end === undefined) ? argArray.length : end;

  var joined;
  for (var i = start; i < end; i++) {
    var arg = argArray[i];
    if (!joined) {
      joined = arg;
    }
    else {
      joined = joined.merge(arg);
    }
  }
  return joined;
};

/**
 * For test/debug use only. The output from this function is subject to wanton
 * random change without notice, and should not be relied upon to even exist
 * at some later date.
 */
Object.defineProperty(Argument.prototype, '_summaryJson', {
  get: function() {
    var assignStatus = this.assignment == null ?
            'null' :
            this.assignment.param.name;
    return '<' + this.prefix + ':' + this.text + ':' + this.suffix + '>' +
        ' (a=' + assignStatus + ',' + ' t=' + this.type + ')';
  },
  enumerable: true
});

exports.Argument = Argument;


/**
 * BlankArgument is a marker that the argument wasn't typed but is there to
 * fill a slot. Assignments begin with their arg set to a BlankArgument.
 */
function BlankArgument() {
  this.text = '';
  this.prefix = '';
  this.suffix = '';
}

BlankArgument.prototype = Object.create(Argument.prototype);

BlankArgument.prototype.type = 'BlankArgument';

exports.BlankArgument = BlankArgument;


/**
 * ScriptArgument is a marker that the argument is designed to be JavaScript.
 * It also implements the special rules that spaces after the { or before the
 * } are part of the pre/suffix rather than the content, and that they are
 * never 'blank' so they can be used by Requisition._split() and not raise an
 * ERROR status due to being blank.
 */
function ScriptArgument(text, prefix, suffix) {
  this.text = text !== undefined ? text : '';
  this.prefix = prefix !== undefined ? prefix : '';
  this.suffix = suffix !== undefined ? suffix : '';

  ScriptArgument._moveSpaces(this);
}

ScriptArgument.prototype = Object.create(Argument.prototype);

ScriptArgument.prototype.type = 'ScriptArgument';

/**
 * Private/Dangerous: Alters a ScriptArgument to move the spaces at the start
 * or end of the 'text' into the prefix/suffix. With a string, " a " is 3 chars
 * long, but with a ScriptArgument, { a } is only one char long.
 * Arguments are generally supposed to be immutable, so this method should only
 * be called on a ScriptArgument that isn't exposed to the outside world yet.
 */
ScriptArgument._moveSpaces = function(arg) {
  while (arg.text.charAt(0) === ' ') {
    arg.prefix = arg.prefix + ' ';
    arg.text = arg.text.substring(1);
  }

  while (arg.text.charAt(arg.text.length - 1) === ' ') {
    arg.suffix = ' ' + arg.suffix;
    arg.text = arg.text.slice(0, -1);
  }
};

/**
 * As Argument.beget that implements the space rule documented in the ctor.
 */
ScriptArgument.prototype.beget = function(options) {
  options.type = ScriptArgument;
  var begotten = Argument.prototype.beget.call(this, options);
  ScriptArgument._moveSpaces(begotten);
  return begotten;
};

exports.ScriptArgument = ScriptArgument;


/**
 * Commands like 'echo' with a single string argument, and used with the
 * special format like: 'echo a b c' effectively have a number of arguments
 * merged together.
 */
function MergedArgument(args, start, end) {
  if (!Array.isArray(args)) {
    throw new Error('args is not an array of Arguments');
  }

  if (start === undefined) {
    this.args = args;
  }
  else {
    this.args = args.slice(start, end);
  }

  var arg = Argument.merge(this.args);
  this.text = arg.text;
  this.prefix = arg.prefix;
  this.suffix = arg.suffix;
}

MergedArgument.prototype = Object.create(Argument.prototype);

MergedArgument.prototype.type = 'MergedArgument';

/**
 * Keep track of which assignment we've been assigned to, and allow the
 * original args to do the same.
 */
Object.defineProperty(MergedArgument.prototype, 'assignment', {
  get: function() { return this._assignment; },
  set: function(assignment) {
    this._assignment = assignment;

    this.args.forEach(function(arg) {
      arg.assignment = assignment;
    }, this);
  },
  enumerable: true
});

MergedArgument.prototype.getArgs = function() {
  return this.args;
};

MergedArgument.prototype.equals = function(that) {
  if (this === that) {
    return true;
  }
  if (that == null || !(that instanceof MergedArgument)) {
    return false;
  }

  // We might need to add a check that args is the same here

  return this.text === that.text &&
       this.prefix === that.prefix && this.suffix === that.suffix;
};

exports.MergedArgument = MergedArgument;


/**
 * TrueNamedArguments are for when we have an argument like --verbose which
 * has a boolean value, and thus the opposite of '--verbose' is ''.
 */
function TrueNamedArgument(arg) {
  this.arg = arg;
  this.text = arg.text;
  this.prefix = arg.prefix;
  this.suffix = arg.suffix;
}

TrueNamedArgument.prototype = Object.create(Argument.prototype);

TrueNamedArgument.prototype.type = 'TrueNamedArgument';

Object.defineProperty(TrueNamedArgument.prototype, 'assignment', {
  get: function() { return this._assignment; },
  set: function(assignment) {
    this._assignment = assignment;

    if (this.arg) {
      this.arg.assignment = assignment;
    }
  },
  enumerable: true
});

TrueNamedArgument.prototype.getArgs = function() {
  return [ this.arg ];
};

TrueNamedArgument.prototype.equals = function(that) {
  if (this === that) {
    return true;
  }
  if (that == null || !(that instanceof TrueNamedArgument)) {
    return false;
  }

  return this.text === that.text &&
       this.prefix === that.prefix && this.suffix === that.suffix;
};

/**
 * As Argument.beget that rebuilds nameArg and valueArg
 */
TrueNamedArgument.prototype.beget = function(options) {
  if (options.text) {
    console.error('Can\'t change text of a TrueNamedArgument', this, options);
  }

  options.type = TrueNamedArgument;
  var begotten = Argument.prototype.beget.call(this, options);
  begotten.arg = new Argument(begotten.text, begotten.prefix, begotten.suffix);
  return begotten;
};

exports.TrueNamedArgument = TrueNamedArgument;


/**
 * FalseNamedArguments are for when we don't have an argument like --verbose
 * which has a boolean value, and thus the opposite of '' is '--verbose'.
 */
function FalseNamedArgument() {
  this.text = '';
  this.prefix = '';
  this.suffix = '';
}

FalseNamedArgument.prototype = Object.create(Argument.prototype);

FalseNamedArgument.prototype.type = 'FalseNamedArgument';

FalseNamedArgument.prototype.getArgs = function() {
  return [ ];
};

FalseNamedArgument.prototype.equals = function(that) {
  if (this === that) {
    return true;
  }
  if (that == null || !(that instanceof FalseNamedArgument)) {
    return false;
  }

  return this.text === that.text &&
       this.prefix === that.prefix && this.suffix === that.suffix;
};

exports.FalseNamedArgument = FalseNamedArgument;


/**
 * A named argument is for cases where we have input in one of the following
 * formats:
 * <ul>
 * <li>--param value
 * <li>-p value
 * </ul>
 * We model this as a normal argument but with a long prefix.
 *
 * There are 2 ways to construct a NamedArgument. One using 2 Arguments which
 * are taken to be the argument for the name (e.g. '--param') and one for the
 * value to assign to that parameter.
 * Alternatively, you can pass in the text/prefix/suffix values in the same
 * way as an Argument is constructed. If you do this then you are expected to
 * assign to nameArg and valueArg before exposing the new NamedArgument.
 */
function NamedArgument() {
  if (typeof arguments[0] === 'string') {
    this.nameArg = null;
    this.valueArg = null;
    this.text = arguments[0];
    this.prefix = arguments[1];
    this.suffix = arguments[2];
  }
  else if (arguments[1] == null) {
    this.nameArg = arguments[0];
    this.valueArg = null;
    this.text = '';
    this.prefix = this.nameArg.toString();
    this.suffix = '';
  }
  else {
    this.nameArg = arguments[0];
    this.valueArg = arguments[1];
    this.text = this.valueArg.text;
    this.prefix = this.nameArg.toString() + this.valueArg.prefix;
    this.suffix = this.valueArg.suffix;
  }
}

NamedArgument.prototype = Object.create(Argument.prototype);

NamedArgument.prototype.type = 'NamedArgument';

Object.defineProperty(NamedArgument.prototype, 'assignment', {
  get: function() { return this._assignment; },
  set: function(assignment) {
    this._assignment = assignment;

    this.nameArg.assignment = assignment;
    if (this.valueArg != null) {
      this.valueArg.assignment = assignment;
    }
  },
  enumerable: true
});

NamedArgument.prototype.getArgs = function() {
  return this.valueArg ? [ this.nameArg, this.valueArg ] : [ this.nameArg ];
};

NamedArgument.prototype.equals = function(that) {
  if (this === that) {
    return true;
  }
  if (that == null) {
    return false;
  }

  if (!(that instanceof NamedArgument)) {
    return false;
  }

  // We might need to add a check that nameArg and valueArg are the same

  return this.text === that.text &&
       this.prefix === that.prefix && this.suffix === that.suffix;
};

/**
 * As Argument.beget that rebuilds nameArg and valueArg
 */
NamedArgument.prototype.beget = function(options) {
  options.type = NamedArgument;
  var begotten = Argument.prototype.beget.call(this, options);

  // Cut the prefix into |whitespace|non-whitespace|whitespace+quote so we can
  // rebuild nameArg and valueArg from the parts
  var matches = /^([\s]*)([^\s]*)([\s]*['"]?)$/.exec(begotten.prefix);

  if (this.valueArg == null && begotten.text === '') {
    begotten.nameArg = new Argument(matches[2], matches[1], matches[3]);
    begotten.valueArg = null;
  }
  else {
    begotten.nameArg = new Argument(matches[2], matches[1], '');
    begotten.valueArg = new Argument(begotten.text, matches[3], begotten.suffix);
  }

  return begotten;
};

exports.NamedArgument = NamedArgument;


/**
 * An argument the groups together a number of plain arguments together so they
 * can be jointly assigned to a single array parameter
 */
function ArrayArgument() {
  this.args = [];
}

ArrayArgument.prototype = Object.create(Argument.prototype);

ArrayArgument.prototype.type = 'ArrayArgument';

ArrayArgument.prototype.addArgument = function(arg) {
  this.args.push(arg);
};

ArrayArgument.prototype.addArguments = function(args) {
  Array.prototype.push.apply(this.args, args);
};

ArrayArgument.prototype.getArguments = function() {
  return this.args;
};

Object.defineProperty(ArrayArgument.prototype, 'assignment', {
  get: function() { return this._assignment; },
  set: function(assignment) {
    this._assignment = assignment;

    this.args.forEach(function(arg) {
      arg.assignment = assignment;
    }, this);
  },
  enumerable: true
});

ArrayArgument.prototype.getArgs = function() {
  return this.args;
};

ArrayArgument.prototype.equals = function(that) {
  if (this === that) {
    return true;
  }
  if (that == null) {
    return false;
  }

  if (that.type !== 'ArrayArgument') {
    return false;
  }

  if (this.args.length !== that.args.length) {
    return false;
  }

  for (var i = 0; i < this.args.length; i++) {
    if (!this.args[i].equals(that.args[i])) {
      return false;
    }
  }

  return true;
};

/**
 * Helper when we're putting arguments back together
 */
ArrayArgument.prototype.toString = function() {
  return '{' + this.args.map(function(arg) {
    return arg.toString();
  }, this).join(',') + '}';
};

exports.ArrayArgument = ArrayArgument;

/**
 * Some types can detect validity, that is to say they can distinguish between
 * valid and invalid values.
 * We might want to change these constants to be numbers for better performance
 */
var Status = {
  /**
   * The conversion process worked without any problem, and the value is
   * valid. There are a number of failure states, so the best way to check
   * for failure is (x !== Status.VALID)
   */
  VALID: {
    toString: function() { return 'VALID'; },
    valueOf: function() { return 0; }
  },

  /**
   * A conversion process failed, however it was noted that the string
   * provided to 'parse()' could be VALID by the addition of more characters,
   * so the typing may not be actually incorrect yet, just unfinished.
   * @see Status.ERROR
   */
  INCOMPLETE: {
    toString: function() { return 'INCOMPLETE'; },
    valueOf: function() { return 1; }
  },

  /**
   * The conversion process did not work, the value should be null and a
   * reason for failure should have been provided. In addition some
   * completion values may be available.
   * @see Status.INCOMPLETE
   */
  ERROR: {
    toString: function() { return 'ERROR'; },
    valueOf: function() { return 2; }
  },

  /**
   * A combined status is the worser of the provided statuses. The statuses
   * can be provided either as a set of arguments or a single array
   */
  combine: function() {
    var combined = Status.VALID;
    for (var i = 0; i < arguments.length; i++) {
      var status = arguments[i];
      if (Array.isArray(status)) {
        status = Status.combine.apply(null, status);
      }
      if (status > combined) {
        combined = status;
      }
    }
    return combined;
  },

  fromString: function(str) {
    switch (str) {
      case Status.VALID.toString():
        return Status.VALID;
      case Status.INCOMPLETE.toString():
        return Status.INCOMPLETE;
      case Status.ERROR.toString():
        return Status.ERROR;
      default:
        throw new Error('\'' + str + '\' is not a status');
    }
  }
};

exports.Status = Status;


/**
 * The type.parse() method converts an Argument into a value, Conversion is
 * a wrapper to that value.
 * Conversion is needed to collect a number of properties related to that
 * conversion in one place, i.e. to handle errors and provide traceability.
 * @param value The result of the conversion. null if status == VALID
 * @param arg The data from which the conversion was made
 * @param status See the Status values [VALID|INCOMPLETE|ERROR] defined above.
 * The default status is Status.VALID.
 * @param message If status=ERROR, there should be a message to describe the
 * error. A message is not needed unless for other statuses, but could be
 * present for any status including VALID (in the case where we want to note a
 * warning, for example).
 * See BUG 664676: GCLI conversion error messages should be localized
 * @param predictions If status=INCOMPLETE, there could be predictions as to
 * the options available to complete the input.
 * We generally expect there to be about 7 predictions (to match human list
 * comprehension ability) however it is valid to provide up to about 20,
 * or less. It is the job of the predictor to decide a smart cut-off.
 * For example if there are 4 very good matches and 4 very poor ones,
 * probably only the 4 very good matches should be presented.
 * The predictions are presented either as an array of prediction objects or as
 * a function which returns this array when called with no parameters.
 * Each prediction object has the following shape:
 *     {
 *       name: '...',     // textual completion. i.e. what the cli uses
 *       value: { ... },  // value behind the textual completion
 *       incomplete: true // this completion is only partial (optional)
 *     }
 * The 'incomplete' property could be used to denote a valid completion which
 * could have sub-values (e.g. for tree navigation).
 */
function Conversion(value, arg, status, message, predictions) {
  if (arg == null) {
    throw new Error('Missing arg');
  }

  if (predictions != null && typeof predictions !== 'function' &&
      !Array.isArray(predictions) && typeof predictions.then !== 'function') {
    throw new Error('predictions exists but is not a promise, function or array');
  }

  if (status === Status.ERROR && !message) {
    throw new Error('Conversion has status=ERROR but no message');
  }

  this.value = value;
  this.arg = arg;
  this._status = status || Status.VALID;
  this.message = message;
  this.predictions = predictions;
}

/**
 * Ensure that all arguments that are part of this conversion know what they
 * are assigned to.
 * @param assignment The Assignment (param/conversion link) to inform the
 * argument about.
 */
Object.defineProperty(Conversion.prototype, 'assignment', {
  get: function() { return this.arg.assignment; },
  set: function(assignment) { this.arg.assignment = assignment; },
  enumerable: true
});

/**
 * Work out if there is information provided in the contained argument.
 */
Conversion.prototype.isDataProvided = function() {
  return this.arg.type !== 'BlankArgument';
};

/**
 * 2 conversions are equal if and only if their args are equal (argEquals) and
 * their values are equal (valueEquals).
 * @param that The conversion object to compare against.
 */
Conversion.prototype.equals = function(that) {
  if (this === that) {
    return true;
  }
  if (that == null) {
    return false;
  }
  return this.valueEquals(that) && this.argEquals(that);
};

/**
 * Check that the value in this conversion is strict equal to the value in the
 * provided conversion.
 * @param that The conversion to compare values with
 */
Conversion.prototype.valueEquals = function(that) {
  return that != null && this.value === that.value;
};

/**
 * Check that the argument in this conversion is equal to the value in the
 * provided conversion as defined by the argument (i.e. arg.equals).
 * @param that The conversion to compare arguments with
 */
Conversion.prototype.argEquals = function(that) {
  return that == null ? false : this.arg.equals(that.arg);
};

/**
 * Accessor for the status of this conversion
 */
Conversion.prototype.getStatus = function(arg) {
  return this._status;
};

/**
 * Defined by the toString() value provided by the argument
 */
Conversion.prototype.toString = function() {
  return this.arg.toString();
};

/**
 * If status === INCOMPLETE, then we may be able to provide predictions as to
 * how the argument can be completed.
 * @return An array of items, or a promise of an array of items, where each
 * item is an object with the following properties:
 * - name (mandatory): Displayed to the user, and typed in. No whitespace
 * - description (optional): Short string for display in a tool-tip
 * - manual (optional): Longer description which details usage
 * - incomplete (optional): Indicates that the prediction if used should not
 *   be considered necessarily sufficient, which typically will mean that the
 *   UI should not append a space to the completion
 * - value (optional): If a value property is present, this will be used as the
 *   value of the conversion, otherwise the item itself will be used.
 */
Conversion.prototype.getPredictions = function(context) {
  if (typeof this.predictions === 'function') {
    return this.predictions(context);
  }
  return Promise.resolve(this.predictions || []);
};

/**
 * Return a promise of an index constrained by the available predictions.
 * i.e. (index % predicitons.length)
 * This code can probably be removed when the Firefox developer toolbar isn't
 * needed any more.
 */
Conversion.prototype.constrainPredictionIndex = function(context, index) {
  if (index == null) {
    return Promise.resolve();
  }

  return this.getPredictions(context).then(function(value) {
    if (value.length === 0) {
      return undefined;
    }

    index = index % value.length;
    if (index < 0) {
      index = value.length + index;
    }
    return index;
  }.bind(this));
};

/**
 * Constant to allow everyone to agree on the maximum number of predictions
 * that should be provided. We actually display 1 less than this number.
 */
Conversion.maxPredictions = 9;

exports.Conversion = Conversion;


/**
 * ArrayConversion is a special Conversion, needed because arrays are converted
 * member by member rather then as a whole, which means we can track the
 * conversion if individual array elements. So an ArrayConversion acts like a
 * normal Conversion (which is needed as Assignment requires a Conversion) but
 * it can also be devolved into a set of Conversions for each array member.
 */
function ArrayConversion(conversions, arg) {
  this.arg = arg;
  this.conversions = conversions;
  this.value = conversions.map(function(conversion) {
    return conversion.value;
  }, this);

  this._status = Status.combine(conversions.map(function(conversion) {
    return conversion.getStatus();
  }));

  // This message is just for reporting errors like "not enough values"
  // rather that for problems with individual values.
  this.message = '';

  // Predictions are generally provided by individual values
  this.predictions = [];
}

ArrayConversion.prototype = Object.create(Conversion.prototype);

Object.defineProperty(ArrayConversion.prototype, 'assignment', {
  get: function() { return this._assignment; },
  set: function(assignment) {
    this._assignment = assignment;

    this.conversions.forEach(function(conversion) {
      conversion.assignment = assignment;
    }, this);
  },
  enumerable: true
});

ArrayConversion.prototype.getStatus = function(arg) {
  if (arg && arg.conversion) {
    return arg.conversion.getStatus();
  }
  return this._status;
};

ArrayConversion.prototype.isDataProvided = function() {
  return this.conversions.length > 0;
};

ArrayConversion.prototype.valueEquals = function(that) {
  if (that == null) {
    return false;
  }

  if (!(that instanceof ArrayConversion)) {
    throw new Error('Can\'t compare values with non ArrayConversion');
  }

  if (this.value === that.value) {
    return true;
  }

  if (this.value.length !== that.value.length) {
    return false;
  }

  for (var i = 0; i < this.conversions.length; i++) {
    if (!this.conversions[i].valueEquals(that.conversions[i])) {
      return false;
    }
  }

  return true;
};

ArrayConversion.prototype.toString = function() {
  return '[ ' + this.conversions.map(function(conversion) {
    return conversion.toString();
  }, this).join(', ') + ' ]';
};

exports.ArrayConversion = ArrayConversion;


/**
 * Most of our types are 'static' e.g. there is only one type of 'string',
 * however some types like 'selection' and 'delegate' are customizable.
 * The basic Type type isn't useful, but does provide documentation about what
 * types do.
 */
function Type() {
}

/**
 * Get a JSONable data structure that entirely describes this type
 * @param commandName/paramName The names of the command and parameter which we
 * are remoting to help the server get back to the remoted action.
 */
Type.prototype.getSpec = function(commandName, paramName) {
  throw new Error('Not implemented');
};

/**
 * Convert the given <tt>value</tt> to a string representation.
 * Where possible, there should be round-tripping between values and their
 * string representations.
 * @param value The object to convert into a string
 * @param context An ExecutionContext to allow basic Requisition access
 */
Type.prototype.stringify = function(value, context) {
  throw new Error('Not implemented');
};

/**
 * Convert the given <tt>arg</tt> to an instance of this type.
 * Where possible, there should be round-tripping between values and their
 * string representations.
 * @param arg An instance of <tt>Argument</tt> to convert.
 * @param context An ExecutionContext to allow basic Requisition access
 * @return Conversion
 */
Type.prototype.parse = function(arg, context) {
  throw new Error('Not implemented');
};

/**
 * A convenience method for times when you don't have an argument to parse
 * but instead have a string.
 * @see #parse(arg)
 */
Type.prototype.parseString = function(str, context) {
  return this.parse(new Argument(str), context);
};

/**
 * The plug-in system, and other things need to know what this type is
 * called. The name alone is not enough to fully specify a type. Types like
 * 'selection' and 'delegate' need extra data, however this function returns
 * only the name, not the extra data.
 */
Type.prototype.name = undefined;

/**
 * If there is some concept of a higher value, return it,
 * otherwise return undefined.
 */
Type.prototype.increment = function(value, context) {
  return undefined;
};

/**
 * If there is some concept of a lower value, return it,
 * otherwise return undefined.
 */
Type.prototype.decrement = function(value, context) {
  return undefined;
};

/**
 * The 'blank value' of most types is 'undefined', but there are exceptions;
 * This allows types to specify a better conversion from empty string than
 * 'undefined'.
 * 2 known examples of this are boolean -> false and array -> []
 */
Type.prototype.getBlank = function(context) {
  return new Conversion(undefined, new BlankArgument(), Status.INCOMPLETE, '');
};

/**
 * This is something of a hack for the benefit of DelegateType which needs to
 * be able to lie about it's type for fields to accept it as one of their own.
 * Sub-types can ignore this unless they're DelegateType.
 * @param context An ExecutionContext to allow basic Requisition access
 */
Type.prototype.getType = function(context) {
  return this;
};

/**
 * addItems allows registrations of a number of things. This allows it to know
 * what type of item, and how it should be registered.
 */
Type.prototype.item = 'type';

exports.Type = Type;

/**
 * 'Types' represents a registry of types
 */
function Types() {
  // Invariant: types[name] = type.name
  this._registered = {};
}

exports.Types = Types;

/**
 * Get an array of the names of registered types
 */
Types.prototype.getTypeNames = function() {
  return Object.keys(this._registered);
};

/**
 * Add a new type to the list available to the system.
 * You can pass 2 things to this function - either an instance of Type, in
 * which case we return this instance when #getType() is called with a 'name'
 * that matches type.name.
 * Also you can pass in a constructor (i.e. function) in which case when
 * #getType() is called with a 'name' that matches Type.prototype.name we will
 * pass the typeSpec into this constructor.
 */
Types.prototype.add = function(type) {
  if (typeof type === 'object') {
    if (!type.name) {
      throw new Error('All registered types must have a name');
    }

    if (type instanceof Type) {
      this._registered[type.name] = type;
    }
    else {
      var name = type.name;
      var parent = type.parent;
      type.name = parent;
      delete type.parent;

      this._registered[name] = this.createType(type);

      type.name = name;
      type.parent = parent;
    }
  }
  else if (typeof type === 'function') {
    if (!type.prototype.name) {
      throw new Error('All registered types must have a name');
    }
    this._registered[type.prototype.name] = type;
  }
  else {
    throw new Error('Unknown type: ' + type);
  }
};

/**
 * Remove a type from the list available to the system
 */
Types.prototype.remove = function(type) {
  delete this._registered[type.name];
};

/**
 * Find a previously registered type
 */
Types.prototype.createType = function(typeSpec) {
  if (typeof typeSpec === 'string') {
    typeSpec = { name: typeSpec };
  }

  if (typeof typeSpec !== 'object') {
    throw new Error('Can\'t extract type from ' + typeSpec);
  }

  var NewTypeCtor, newType;
  if (typeSpec.name == null || typeSpec.name == 'type') {
    NewTypeCtor = Type;
  }
  else {
    NewTypeCtor = this._registered[typeSpec.name];
  }

  if (!NewTypeCtor) {
    console.error('Known types: ' + Object.keys(this._registered).join(', '));
    throw new Error('Unknown type: \'' + typeSpec.name + '\'');
  }

  if (typeof NewTypeCtor === 'function') {
    newType = new NewTypeCtor(typeSpec);
  }
  else {
    // clone 'type'
    newType = {};
    util.copyProperties(NewTypeCtor, newType);
  }

  // Copy the properties of typeSpec onto the new type
  util.copyProperties(typeSpec, newType);

  // Several types need special powers to create child types
  newType.types = this;

  if (typeof NewTypeCtor !== 'function') {
    if (typeof newType.constructor === 'function') {
      newType.constructor();
    }
  }

  return newType;
};
