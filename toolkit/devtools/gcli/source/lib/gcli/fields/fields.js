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

/**
 * A Field is a way to get input for a single parameter.
 * This class is designed to be inherited from. It's important that all
 * subclasses have a similar constructor signature because they are created
 * via Fields.get(...)
 * @param type The type to use in conversions
 * @param options A set of properties to help fields configure themselves:
 * - document: The document we use in calling createElement
 * - requisition: The requisition that we're attached to
 */
function Field(type, options) {
  this.type = type;
  this.document = options.document;
  this.requisition = options.requisition;
}

/**
 * Enable registration of fields using addItems
 */
Field.prototype.item = 'field';

/**
 * Subclasses should assign their element with the DOM node that gets added
 * to the 'form'. It doesn't have to be an input node, just something that
 * contains it.
 */
Field.prototype.element = undefined;

/**
 * Called from the outside to indicate that the command line has changed and
 * the field should update itself
 */
Field.prototype.update = function() {
};

/**
 * Indicates that this field should drop any resources that it has created
 */
Field.prototype.destroy = function() {
  this.messageElement = undefined;
  this.document = undefined;
  this.requisition = undefined;
};

// Note: We could/should probably change Fields from working with Conversions
// to working with Arguments (Tokens), which makes for less calls to parse()

/**
 * Update this field display with the value from this conversion.
 * Subclasses should provide an implementation of this function.
 */
Field.prototype.setConversion = function(conversion) {
  throw new Error('Field should not be used directly');
};

/**
 * Extract a conversion from the values in this field.
 * Subclasses should provide an implementation of this function.
 */
Field.prototype.getConversion = function() {
  throw new Error('Field should not be used directly');
};

/**
 * Set the element where messages and validation errors will be displayed
 * @see setMessage()
 */
Field.prototype.setMessageElement = function(element) {
  this.messageElement = element;
};

/**
 * Display a validation message in the UI
 */
Field.prototype.setMessage = function(message) {
  if (this.messageElement) {
    util.setTextContent(this.messageElement, message || '');
  }
};

/**
 * Some fields contain information that is more important to the user, for
 * example error messages and completion menus.
 */
Field.prototype.isImportant = false;

/**
 * 'static/abstract' method to allow implementations of Field to lay a claim
 * to a type. This allows claims of various strength to be weighted up.
 * See the Field.*MATCH values.
 */
Field.claim = function(type, context) {
  throw new Error('Field should not be used directly');
};

/**
 * How good a match is a field for a given type
 */
Field.MATCH = 3;           // Good match
Field.DEFAULT = 2;         // A default match
Field.BASIC = 1;           // OK in an emergency. i.e. assume Strings
Field.NO_MATCH = 0;        // This field can't help with the given type

exports.Field = Field;


/**
 * A manager for the registered Fields
 */
function Fields() {
  // Internal array of known fields
  this._fieldCtors = [];
}

/**
 * Add a field definition by field constructor
 * @param fieldCtor Constructor function of new Field
 */
Fields.prototype.add = function(fieldCtor) {
  if (typeof fieldCtor !== 'function') {
    console.error('fields.add erroring on ', fieldCtor);
    throw new Error('fields.add requires a Field constructor');
  }
  this._fieldCtors.push(fieldCtor);
};

/**
 * Remove a Field definition
 * @param field A previously registered field, specified either with a field
 * name or from the field name
 */
Fields.prototype.remove = function(field) {
  if (typeof field !== 'string') {
    this._fieldCtors = this._fieldCtors.filter(function(test) {
      return test !== field;
    });
  }
  else if (field instanceof Field) {
    this.remove(field.name);
  }
  else {
    console.error('fields.remove erroring on ', field);
    throw new Error('fields.remove requires an instance of Field');
  }
};

/**
 * Find the best possible matching field from the specification of the type
 * of field required.
 * @param type An instance of Type that we will represent
 * @param options A set of properties that we should attempt to match, and use
 * in the construction of the new field object:
 * - document: The document to use in creating new elements
 * - requisition: The requisition we're monitoring,
 * @return A newly constructed field that best matches the input options
 */
Fields.prototype.get = function(type, options) {
  var FieldConstructor;
  var highestClaim = -1;
  this._fieldCtors.forEach(function(fieldCtor) {
    var context = (options.requisition == null) ?
                  null : options.requisition.executionContext;
    var claim = fieldCtor.claim(type, context);
    if (claim > highestClaim) {
      highestClaim = claim;
      FieldConstructor = fieldCtor;
    }
  });

  if (!FieldConstructor) {
    console.error('Unknown field type ', type, ' in ', this._fieldCtors);
    throw new Error('Can\'t find field for ' + type);
  }

  if (highestClaim < Field.DEFAULT) {
    return new BlankField(type, options);
  }

  return new FieldConstructor(type, options);
};

exports.Fields = Fields;

/**
 * For use with delegate types that do not yet have anything to resolve to.
 * BlankFields are not for general use.
 */
function BlankField(type, options) {
  Field.call(this, type, options);

  this.element = util.createElement(this.document, 'div');

  this.onFieldChange = util.createEvent('BlankField.onFieldChange');
}

BlankField.prototype = Object.create(Field.prototype);

BlankField.claim = function(type, context) {
  return type.name === 'blank' ? Field.MATCH : Field.NO_MATCH;
};

BlankField.prototype.destroy = function() {
  this.element = undefined;
  Field.prototype.destroy.call(this);
};

BlankField.prototype.setConversion = function(conversion) {
  this.setMessage(conversion.message);
};

BlankField.prototype.getConversion = function() {
  return this.type.parseString('', this.requisition.executionContext);
};

/**
 * Items for export
 */
exports.items = [ BlankField ];
