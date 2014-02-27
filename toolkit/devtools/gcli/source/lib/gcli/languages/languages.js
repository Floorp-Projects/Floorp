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
var promise = require('../util/promise');

var RESOLVED = promise.resolve(true);

/**
 * This is where we cache the languages that we know about
 */
var languages = {};

/**
 * This is the base implementation for all languages
 */
var baseLanguage = {
  item: 'language',
  name: undefined,

  constructor: function(terminal) {
  },

  destroy: function() {
  },

  updateHints: function() {
    util.clearElement(this.terminal.tooltipElement);
  },

  description: '',
  message: '',
  caretMoved: function() {},

  handleUpArrow: function() {
    return promise.resolve(false);
  },

  handleDownArrow: function() {
    return promise.resolve(false);
  },

  handleTab: function() {
    this.terminal.unsetChoice();
    return RESOLVED;
  },

  handleInput: function(input) {
    if (input === ':') {
      return this.terminal.setInput('').then(function() {
        return this.terminal.pushLanguage('commands');
      }.bind(this));
    }

    this.terminal.unsetChoice();
    return RESOLVED;
  },

  handleReturn: function(input) {
    var rowoutEle = this.document.createElement('pre');
    rowoutEle.classList.add('gcli-row-out');
    rowoutEle.classList.add('gcli-row-script');
    rowoutEle.setAttribute('aria-live', 'assertive');

    return this.exec(input).then(function(line) {
      rowoutEle.innerHTML = line;

      this.terminal.addElement(rowoutEle);
      this.terminal.scrollToBottom();

      this.focusManager.outputted();

      this.terminal.unsetChoice();
      this.terminal.inputElement.value = '';
    }.bind(this));
  },

  setCursor: function(cursor) {
    this.terminal.inputElement.selectionStart = cursor.start;
    this.terminal.inputElement.selectionEnd = cursor.end;
  },

  getCompleterTemplateData: function() {
    return promise.resolve({
      statusMarkup: [
        {
          string: this.terminal.inputElement.value.replace(/ /g, '\u00a0'), // i.e. &nbsp;
          className: 'gcli-in-valid'
        }
      ],
      unclosedJs: false,
      directTabText: '',
      arrowTabText: '',
      emptyParameters: ''
    });
  },

  showIntro: function() {
  },

  exec: function(input) {
    throw new Error('Missing implementation of handleReturn() or exec() ' + this.name);
  }
};

/**
 * Add a new language to the cache
 */
exports.addLanguage = function(language) {
  languages[language.name] = language;
};

/**
 * Remove an existing language from the cache
 */
exports.removeLanguage = function(language) {
  var name = typeof language === 'string' ? language : language.name;
  delete languages[name];
};

/**
 * Get access to the list of known languages
 */
exports.getLanguages = function() {
  return Object.keys(languages).map(function(name) {
    return languages[name];
  });
};

/**
 * Find a type, previously registered using #addType()
 */
exports.createLanguage = function(name, terminal) {
  if (name == null) {
    name = Object.keys(languages)[0];
  }

  var language = (typeof name === 'string') ? languages[name] : name;
  if (!language) {
    console.error('Known languages: ' + Object.keys(languages).join(', '));
    throw new Error('Unknown language: \'' + name + '\'');
  }

  // clone 'type'
  var newInstance = {};
  util.copyProperties(baseLanguage, newInstance);
  util.copyProperties(language, newInstance);

  if (typeof newInstance.constructor === 'function') {
    var reply = newInstance.constructor(terminal);
    return promise.resolve(reply).then(function() {
      return newInstance;
    });
  }
  else {
    return promise.resolve(newInstance);
  }
};
