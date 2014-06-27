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
var domtemplate = require('../util/domtemplate');
var host = require('../util/host');

var Status = require('../types/types').Status;
var cli = require('../cli');
var Requisition = require('../cli').Requisition;
var CommandAssignment = require('../cli').CommandAssignment;
var fields = require('../fields/fields');
var intro = require('../ui/intro');

var RESOLVED = Promise.resolve(true);

/**
 * Various ways in which we need to manipulate the caret/selection position.
 * A value of null means we're not expecting a change
 */
var Caret = exports.Caret = {
  /**
   * We are expecting changes, but we don't need to move the cursor
   */
  NO_CHANGE: 0,

  /**
   * We want the entire input area to be selected
   */
  SELECT_ALL: 1,

  /**
   * The whole input has changed - push the cursor to the end
   */
  TO_END: 2,

  /**
   * A part of the input has changed - push the cursor to the end of the
   * changed section
   */
  TO_ARG_END: 3
};

/**
 * Shared promise for loading command.html
 */
var commandHtmlPromise;

var commandLanguage = exports.commandLanguage = {
  // Language implementation for GCLI commands
  item: 'language',
  name: 'commands',
  prompt: ':',
  proportionalFonts: true,

  constructor: function(terminal) {
    this.terminal = terminal;
    this.document = terminal.document;
    this.focusManager = this.terminal.focusManager;

    var options = this.terminal.options;
    this.requisition = options.requisition;
    if (this.requisition == null) {
      if (options.environment == null) {
        options.environment = {};
        options.environment.document = options.document || this.document;
        options.environment.window = options.environment.document.defaultView;
      }

      this.requisition = new Requisition(options);
    }

    // We also keep track of the last known arg text for the current assignment
    this.lastText = undefined;

    // Used to effect caret changes. See _processCaretChange()
    this._caretChange = null;

    // We keep track of which assignment the cursor is in
    this.assignment = this.requisition.getAssignmentAt(0);

    if (commandHtmlPromise == null) {
      commandHtmlPromise = host.staticRequire(module, './command.html');
    }

    return commandHtmlPromise.then(function(commandHtml) {
      this.commandDom = host.toDom(this.document, commandHtml);

      this.requisition.commandOutputManager.onOutput.add(this.outputted, this);
      var mapping = cli.getMapping(this.requisition.executionContext);
      mapping.terminal = this.terminal;

      this.requisition.onExternalUpdate.add(this.textChanged, this);

      return this;
    }.bind(this));
  },

  destroy: function() {
    var mapping = cli.getMapping(this.requisition.executionContext);
    delete mapping.terminal;

    this.requisition.commandOutputManager.onOutput.remove(this.outputted, this);
    this.requisition.onExternalUpdate.remove(this.textChanged, this);

    this.terminal = undefined;
    this.requisition = undefined;
    this.commandDom = undefined;
  },

  // From the requisition.textChanged event
  textChanged: function() {
    if (this.terminal == null) {
      return; // This can happen post-destroy()
    }

    if (this.terminal._caretChange == null) {
      // We weren't expecting a change so this was requested by the hint system
      // we should move the cursor to the end of the 'changed section', and the
      // best we can do for that right now is the end of the current argument.
      this.terminal._caretChange = Caret.TO_ARG_END;
    }

    var newStr = this.requisition.toString();
    var input = this.terminal.getInputState();

    input.typed = newStr;
    this._processCaretChange(input);

    // We don't update terminal._previousValue. Should we?
    // Shouldn't this really be a function of terminal?
    if (this.terminal.inputElement.value !== newStr) {
      this.terminal.inputElement.value = newStr;
    }
    this.terminal.onInputChange({ inputState: input });

    // We get here for minor things like whitespace change in arg prefix,
    // so we ignore anything but an actual value change.
    if (this.assignment.arg.text === this.lastText) {
      return;
    }

    this.lastText = this.assignment.arg.text;

    this.terminal.field.update();
    this.terminal.field.setConversion(this.assignment.conversion);
    util.setTextContent(this.terminal.descriptionEle, this.description);
  },

  // Called internally whenever we think that the current assignment might
  // have changed, typically on mouse-clicks or key presses.
  caretMoved: function(start) {
    if (!this.requisition.isUpToDate()) {
      return;
    }
    var newAssignment = this.requisition.getAssignmentAt(start);
    if (newAssignment == null) {
      return;
    }

    if (this.assignment !== newAssignment) {
      if (this.assignment.param.type.onLeave) {
        this.assignment.param.type.onLeave(this.assignment);
      }

      // This can be kicked off either by requisition doing an assign or by
      // terminal noticing a cursor movement out of a command, so we should
      // check that this really is a new assignment
      var isNew = (this.assignment !== newAssignment);

      this.assignment = newAssignment;
      this.terminal.updateCompletion();

      if (isNew) {
        this.updateHints();
      }

      if (this.assignment.param.type.onEnter) {
        this.assignment.param.type.onEnter(this.assignment);
      }
    }
    else {
      if (this.assignment && this.assignment.param.type.onChange) {
        this.assignment.param.type.onChange(this.assignment);
      }
    }

    // Warning: compare the logic here with the logic in fieldChanged, which
    // is slightly different. They should probably be the same
    var error = (this.assignment.status === Status.ERROR);
    this.focusManager.setError(error);
  },

  // Called whenever the assignment that we're providing help with changes
  updateHints: function() {
    this.lastText = this.assignment.arg.text;

    var field = this.terminal.field;
    if (field) {
      field.onFieldChange.remove(this.terminal.fieldChanged, this.terminal);
      field.destroy();
    }

    field = this.terminal.field = fields.getField(this.assignment.param.type, {
      document: this.terminal.document,
      requisition: this.requisition
    });

    this.focusManager.setImportantFieldFlag(field.isImportant);

    field.onFieldChange.add(this.terminal.fieldChanged, this.terminal);
    field.setConversion(this.assignment.conversion);

    // Filled in by the template process
    this.terminal.errorEle = undefined;
    this.terminal.descriptionEle = undefined;

    var contents = this.terminal.tooltipTemplate.cloneNode(true);
    domtemplate.template(contents, this.terminal, {
      blankNullUndefined: true,
      stack: 'terminal.html#tooltip'
    });

    util.clearElement(this.terminal.tooltipElement);
    this.terminal.tooltipElement.appendChild(contents);
    this.terminal.tooltipElement.style.display = 'block';

    field.setMessageElement(this.terminal.errorEle);
  },

  /**
   * See also handleDownArrow for some symmetry
   */
  handleUpArrow: function() {
    // If the user is on a valid value, then we increment the value, but if
    // they've typed something that's not right we page through predictions
    if (this.assignment.getStatus() === Status.VALID) {
      return this.requisition.increment(this.assignment).then(function() {
        this.textChanged();
        this.focusManager.onInputChange();
        return true;
      }.bind(this));
    }

    return Promise.resolve(false);
  },

  /**
   * See also handleUpArrow for some symmetry
   */
  handleDownArrow: function() {
    if (this.assignment.getStatus() === Status.VALID) {
      return this.requisition.decrement(this.assignment).then(function() {
        this.textChanged();
        this.focusManager.onInputChange();
        return true;
      }.bind(this));
    }

    return Promise.resolve(false);
  },

  /**
   * RETURN checks status and might exec
   */
  handleReturn: function(input) {
    // Deny RETURN unless the command might work
    if (this.requisition.status !== Status.VALID) {
      return Promise.resolve(false);
    }

    this.terminal.history.add(input);
    this.terminal.unsetChoice();

    return this.requisition.exec().then(function() {
      this.textChanged();
      return true;
    }.bind(this));
  },

  /**
   * Warning: We get TAB events for more than just the user pressing TAB in our
   * input element.
   */
  handleTab: function() {
    // It's possible for TAB to not change the input, in which case the
    // textChanged event will not fire, and the caret move will not be
    // processed. So we check that this is done first
    this.terminal._caretChange = Caret.TO_ARG_END;
    var inputState = this.terminal.getInputState();
    this._processCaretChange(inputState);

    this.terminal._previousValue = this.terminal.inputElement.value;

    // The changes made by complete may happen asynchronously, so after the
    // the call to complete() we should avoid making changes before the end
    // of the event loop
    var index = this.terminal.getChoiceIndex();
    return this.requisition.complete(inputState.cursor, index).then(function(updated) {
      // Abort UI changes if this UI update has been overtaken
      if (!updated) {
        return RESOLVED;
      }
      this.textChanged();
      return this.terminal.unsetChoice();
    }.bind(this));
  },

  /**
   * The input text has changed in some way.
   */
  handleInput: function(value) {
    this.terminal._caretChange = Caret.NO_CHANGE;

    return this.requisition.update(value).then(function(updated) {
      // Abort UI changes if this UI update has been overtaken
      if (!updated) {
        return RESOLVED;
      }
      this.textChanged();
      return this.terminal.unsetChoice();
    }.bind(this));
  },

  /**
   * Counterpart to |setInput| for moving the cursor.
   * @param cursor A JS object shaped like { start: x, end: y }
   */
  setCursor: function(cursor) {
    this._caretChange = Caret.NO_CHANGE;
    this._processCaretChange({
      typed: this.terminal.inputElement.value,
      cursor: cursor
    });
  },

  /**
   * If this._caretChange === Caret.TO_ARG_END, we alter the input object to move
   * the selection start to the end of the current argument.
   * @param input An object shaped like { typed:'', cursor: { start:0, end:0 }}
   */
  _processCaretChange: function(input) {
    var start, end;
    switch (this._caretChange) {
      case Caret.SELECT_ALL:
        start = 0;
        end = input.typed.length;
        break;

      case Caret.TO_END:
        start = input.typed.length;
        end = input.typed.length;
        break;

      case Caret.TO_ARG_END:
        // There could be a fancy way to do this involving assignment/arg math
        // but it doesn't seem easy, so we cheat a move the cursor to just before
        // the next space, or the end of the input
        start = input.cursor.start;
        do {
          start++;
        }
        while (start < input.typed.length && input.typed[start - 1] !== ' ');

        end = start;
        break;

      default:
        start = input.cursor.start;
        end = input.cursor.end;
        break;
    }

    start = (start > input.typed.length) ? input.typed.length : start;
    end = (end > input.typed.length) ? input.typed.length : end;

    var newInput = {
      typed: input.typed,
      cursor: { start: start, end: end }
    };

    if (this.terminal.inputElement.selectionStart !== start) {
      this.terminal.inputElement.selectionStart = start;
    }
    if (this.terminal.inputElement.selectionEnd !== end) {
      this.terminal.inputElement.selectionEnd = end;
    }

    this.caretMoved(start);

    this._caretChange = null;
    return newInput;
  },

  /**
   * Calculate the properties required by the template process for completer.html
   */
  getCompleterTemplateData: function() {
    var input = this.terminal.getInputState();
    var start = input.cursor.start;
    var index = this.terminal.getChoiceIndex();

    return this.requisition.getStateData(start, index).then(function(data) {
      // Calculate the statusMarkup required to show wavy lines underneath the
      // input text (like that of an inline spell-checker) which used by the
      // template process for completer.html
      // i.e. s/space/&nbsp/g in the string (for HTML display) and status to an
      // appropriate class name (i.e. lower cased, prefixed with gcli-in-)
      data.statusMarkup.forEach(function(member) {
        member.string = member.string.replace(/ /g, '\u00a0'); // i.e. &nbsp;
        member.className = 'gcli-in-' + member.status.toString().toLowerCase();
      }, this);

      return data;
    });
  },

  /**
   * Called by the onFieldChange event (via the terminal) on the current Field
   */
  fieldChanged: function(ev) {
    this.requisition.setAssignment(this.assignment, ev.conversion.arg,
                                   { matchPadding: true }).then(function() {
      this.textChanged();
    }.bind(this));

    var isError = ev.conversion.message != null && ev.conversion.message !== '';
    this.focusManager.setError(isError);
  },

  /**
   * Monitor for new command executions
   */
  outputted: function(ev) {
    if (ev.output.hidden) {
      return;
    }

    var template = this.commandDom.cloneNode(true);
    var templateOptions = { stack: 'terminal.html#outputView' };

    var context = this.requisition.conversionContext;
    var data = {
      onclick: context.update,
      ondblclick: context.updateExec,
      language: this,
      output: ev.output,
      promptClass: (ev.output.error ? 'gcli-row-error' : '') +
                   (ev.output.completed ? ' gcli-row-complete' : ''),
      // Elements attached to this by template().
      rowinEle: null,
      rowoutEle: null,
      throbEle: null,
      promptEle: null
    };

    domtemplate.template(template, data, templateOptions);

    ev.output.promise.then(function() {
      var document = data.rowoutEle.ownerDocument;

      if (ev.output.completed) {
        data.promptEle.classList.add('gcli-row-complete');
      }
      if (ev.output.error) {
        data.promptEle.classList.add('gcli-row-error');
      }

      util.clearElement(data.rowoutEle);

      return ev.output.convert('dom', context).then(function(node) {
        this._linksToNewTab(node);
        data.rowoutEle.appendChild(node);

        var event = document.createEvent('Event');
        event.initEvent('load', true, true);
        event.addedElement = node;
        node.dispatchEvent(event);

        this.terminal.scrollToBottom();
        data.throbEle.style.display = ev.output.completed ? 'none' : 'block';
      }.bind(this));
    }.bind(this)).then(null, console.error);

    this.terminal.addElement(data.rowinEle);
    this.terminal.addElement(data.rowoutEle);
    this.terminal.scrollToBottom();

    this.focusManager.outputted();
  },

  /**
   * Find elements with href attributes and add a target=_blank so opened links
   * will open in a new window
   */
  _linksToNewTab: function(element) {
    var links = element.querySelectorAll('*[href]');
    for (var i = 0; i < links.length; i++) {
      links[i].setAttribute('target', '_blank');
    }
    return element;
  },

  /**
   * Show a short introduction to this language
   */
  showIntro: function() {
    intro.maybeShowIntro(this.requisition.commandOutputManager,
                         this.requisition.conversionContext);
  },
};

/**
 * The description (displayed at the top of the hint area) should be blank if
 * we're entering the CommandAssignment (because it's obvious) otherwise it's
 * the parameter description.
 */
Object.defineProperty(commandLanguage, 'description', {
  get: function() {
    if (this.assignment == null || (
        this.assignment instanceof CommandAssignment &&
        this.assignment.value == null)) {
      return '';
    }

    return this.assignment.param.manual || this.assignment.param.description;
  },
  enumerable: true
});

/**
 * Present an error message to the hint popup
 */
Object.defineProperty(commandLanguage, 'message', {
  get: function() {
    return this.assignment.conversion.message;
  },
  enumerable: true
});

exports.items = [ commandLanguage ];
