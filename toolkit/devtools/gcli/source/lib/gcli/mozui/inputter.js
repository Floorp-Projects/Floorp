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

var promise = require('../util/promise');
var util = require('../util/util');
var KeyEvent = require('../util/util').KeyEvent;

var Status = require('../types/types').Status;
var History = require('../ui/history').History;

var RESOLVED = promise.resolve(true);

/**
 * A wrapper to take care of the functions concerning an input element
 * @param options Object containing user customization properties, including:
 * - promptWidth (default=22px)
 * @param components Object that links to other UI components. GCLI provided:
 * - requisition
 * - focusManager
 * - element
 */
function Inputter(options, components) {
  this.requisition = components.requisition;
  this.focusManager = components.focusManager;

  this.element = components.element;
  this.element.classList.add('gcli-in-input');
  this.element.spellcheck = false;

  this.document = this.element.ownerDocument;

  // Used to distinguish focus from TAB in CLI. See onKeyUp()
  this.lastTabDownAt = 0;

  // Used to effect caret changes. See _processCaretChange()
  this._caretChange = null;

  // Ensure that TAB/UP/DOWN isn't handled by the browser
  this.onKeyDown = this.onKeyDown.bind(this);
  this.onKeyUp = this.onKeyUp.bind(this);
  this.element.addEventListener('keydown', this.onKeyDown, false);
  this.element.addEventListener('keyup', this.onKeyUp, false);

  // Setup History
  this.history = new History();
  this._scrollingThroughHistory = false;

  // Used when we're selecting which prediction to complete with
  this._choice = null;
  this.onChoiceChange = util.createEvent('Inputter.onChoiceChange');

  // Cursor position affects hint severity
  this.onMouseUp = this.onMouseUp.bind(this);
  this.element.addEventListener('mouseup', this.onMouseUp, false);

  if (this.focusManager) {
    this.focusManager.addMonitoredElement(this.element, 'input');
  }

  // Initially an asynchronous completion isn't in-progress
  this._completed = RESOLVED;

  this.textChanged = this.textChanged.bind(this);

  this.outputted = this.outputted.bind(this);
  this.requisition.commandOutputManager.onOutput.add(this.outputted, this);

  this.assignment = this.requisition.getAssignmentAt(0);
  this.onAssignmentChange = util.createEvent('Inputter.onAssignmentChange');
  this.onInputChange = util.createEvent('Inputter.onInputChange');

  this.onResize = util.createEvent('Inputter.onResize');
  this.onWindowResize = this.onWindowResize.bind(this);
  this.document.defaultView.addEventListener('resize', this.onWindowResize, false);

  this._previousValue = undefined;
  this.requisition.update(this.element.value || '');
}

/**
 * Avoid memory leaks
 */
Inputter.prototype.destroy = function() {
  this.document.defaultView.removeEventListener('resize', this.onWindowResize, false);

  this.requisition.commandOutputManager.onOutput.remove(this.outputted, this);
  if (this.focusManager) {
    this.focusManager.removeMonitoredElement(this.element, 'input');
  }

  this.element.removeEventListener('mouseup', this.onMouseUp, false);
  this.element.removeEventListener('keydown', this.onKeyDown, false);
  this.element.removeEventListener('keyup', this.onKeyUp, false);

  this.history.destroy();

  if (this.style) {
    this.style.parentNode.removeChild(this.style);
    this.style = undefined;
  }

  this.textChanged = undefined;
  this.outputted = undefined;
  this.onMouseUp = undefined;
  this.onKeyDown = undefined;
  this.onKeyUp = undefined;
  this.onWindowResize = undefined;
  this.tooltip = undefined;
  this.document = undefined;
  this.element = undefined;
};

/**
 * Make ourselves visually similar to the input element, and make the input
 * element transparent so our background shines through
 */
Inputter.prototype.onWindowResize = function() {
  // Mochitest sometimes causes resize after shutdown. See Bug 743190
  if (!this.element) {
    return;
  }

  this.onResize(this.getDimensions());
};

/**
 * Make ourselves visually similar to the input element, and make the input
 * element transparent so our background shines through
 */
Inputter.prototype.getDimensions = function() {
  var fixedLoc = {};
  var currentElement = this.element.parentNode;
  while (currentElement && currentElement.nodeName !== '#document') {
    var style = this.document.defaultView.getComputedStyle(currentElement, '');
    if (style) {
      var position = style.getPropertyValue('position');
      if (position === 'absolute' || position === 'fixed') {
        var bounds = currentElement.getBoundingClientRect();
        fixedLoc.top = bounds.top;
        fixedLoc.left = bounds.left;
        break;
      }
    }
    currentElement = currentElement.parentNode;
  }

  var rect = this.element.getBoundingClientRect();
  return {
    top: rect.top - (fixedLoc.top || 0) + 1,
    height: rect.bottom - rect.top - 1,
    left: rect.left - (fixedLoc.left || 0) + 2,
    width: rect.right - rect.left
  };
};

/**
 * Pass 'outputted' events on to the focus manager
 */
Inputter.prototype.outputted = function() {
  if (this.focusManager) {
    this.focusManager.outputted();
  }
};

/**
 * Handler for the input-element.onMouseUp event
 */
Inputter.prototype.onMouseUp = function(ev) {
  this._checkAssignment();
};

/**
 * Handler for the Requisition.textChanged event
 */
Inputter.prototype.textChanged = function() {
  if (!this.document) {
    return; // This can happen post-destroy()
  }

  if (this._caretChange == null) {
    // We weren't expecting a change so this was requested by the hint system
    // we should move the cursor to the end of the 'changed section', and the
    // best we can do for that right now is the end of the current argument.
    this._caretChange = Caret.TO_ARG_END;
  }

  var newStr = this.requisition.toString();
  var input = this.getInputState();

  input.typed = newStr;
  this._processCaretChange(input);

  if (this.element.value !== newStr) {
    this.element.value = newStr;
  }
  this.onInputChange({ inputState: input });

  this.tooltip.textChanged();
};

/**
 * Various ways in which we need to manipulate the caret/selection position.
 * A value of null means we're not expecting a change
 */
var Caret = {
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
 * If this._caretChange === Caret.TO_ARG_END, we alter the input object to move
 * the selection start to the end of the current argument.
 * @param input An object shaped like { typed:'', cursor: { start:0, end:0 }}
 */
Inputter.prototype._processCaretChange = function(input) {
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

  if (this.element.selectionStart !== start) {
    this.element.selectionStart = start;
  }
  if (this.element.selectionEnd !== end) {
    this.element.selectionEnd = end;
  }

  this._checkAssignment(start);

  this._caretChange = null;
  return newInput;
};

/**
 * To be called internally whenever we think that the current assignment might
 * have changed, typically on mouse-clicks or key presses.
 * @param start Optional - if specified, the cursor position to use in working
 * out the current assignment. This is needed because setting the element
 * selection start is only recognised when the event loop has finished
 */
Inputter.prototype._checkAssignment = function(start) {
  if (start == null) {
    start = this.element.selectionStart;
  }
  var newAssignment = this.requisition.getAssignmentAt(start);
  if (this.assignment !== newAssignment) {
    if (this.assignment.param.type.onLeave) {
      this.assignment.param.type.onLeave(this.assignment);
    }

    this.assignment = newAssignment;
    this.onAssignmentChange({ assignment: this.assignment });

    if (this.assignment.param.type.onEnter) {
      this.assignment.param.type.onEnter(this.assignment);
    }
  }
  else {
    if (this.assignment && this.assignment.param.type.onChange) {
      this.assignment.param.type.onChange(this.assignment);
    }
  }

  // This is slightly nasty - the focusManager generally relies on people
  // telling it what it needs to know (which makes sense because the event
  // system to do it with events would be unnecessarily complex). However
  // requisition doesn't know about the focusManager either. So either one
  // needs to know about the other, or a third-party needs to break the
  // deadlock. These 2 lines are all we're quibbling about, so for now we hack
  if (this.focusManager) {
    var error = (this.assignment.status === Status.ERROR);
    this.focusManager.setError(error);
  }
};

/**
 * Set the input field to a value, for external use.
 * This function updates the data model. It sets the caret to the end of the
 * input. It does not make any similarity checks so calling this function with
 * it's current value resets the cursor position.
 * It does not execute the input or affect the history.
 * This function should not be called internally, by Inputter and never as a
 * result of a keyboard event on this.element or bug 676520 could be triggered.
 */
Inputter.prototype.setInput = function(str) {
  this._caretChange = Caret.TO_END;
  return this.requisition.update(str).then(function(updated) {
    this.textChanged();
    return updated;
  }.bind(this));
};

/**
 * Counterpart to |setInput| for moving the cursor.
 * @param cursor An object shaped like { start: x, end: y }
 */
Inputter.prototype.setCursor = function(cursor) {
  this._caretChange = Caret.NO_CHANGE;
  this._processCaretChange({ typed: this.element.value, cursor: cursor });
  return RESOLVED;
};

/**
 * Focus the input element
 */
Inputter.prototype.focus = function() {
  this.element.focus();
  this._checkAssignment();
};

/**
 * Ensure certain keys (arrows, tab, etc) that we would like to handle
 * are not handled by the browser
 */
Inputter.prototype.onKeyDown = function(ev) {
  if (ev.keyCode === KeyEvent.DOM_VK_UP || ev.keyCode === KeyEvent.DOM_VK_DOWN) {
    ev.preventDefault();
    return;
  }

  // The following keys do not affect the state of the command line so we avoid
  // informing the focusManager about keyboard events that involve these keys
  if (ev.keyCode === KeyEvent.DOM_VK_F1 ||
      ev.keyCode === KeyEvent.DOM_VK_ESCAPE ||
      ev.keyCode === KeyEvent.DOM_VK_UP ||
      ev.keyCode === KeyEvent.DOM_VK_DOWN) {
    return;
  }

  if (this.focusManager) {
    this.focusManager.onInputChange();
  }

  if (ev.keyCode === KeyEvent.DOM_VK_TAB) {
    this.lastTabDownAt = 0;
    if (!ev.shiftKey) {
      ev.preventDefault();
      // Record the timestamp of this TAB down so onKeyUp can distinguish
      // focus from TAB in the CLI.
      this.lastTabDownAt = ev.timeStamp;
    }
    if (ev.metaKey || ev.altKey || ev.crtlKey) {
      if (this.document.commandDispatcher) {
        this.document.commandDispatcher.advanceFocus();
      }
      else {
        this.element.blur();
      }
    }
  }
};

/**
 * Handler for use with DOM events, which just calls the promise enabled
 * handleKeyUp function but checks the exit state of the promise so we know
 * if something went wrong.
 */
Inputter.prototype.onKeyUp = function(ev) {
  this.handleKeyUp(ev).then(null, util.errorHandler);
};

/**
 * The main keyboard processing loop
 * @return A promise that resolves (to undefined) when the actions kicked off
 * by this handler are completed.
 */
Inputter.prototype.handleKeyUp = function(ev) {
  if (this.focusManager && ev.keyCode === KeyEvent.DOM_VK_F1) {
    this.focusManager.helpRequest();
    return RESOLVED;
  }

  if (this.focusManager && ev.keyCode === KeyEvent.DOM_VK_ESCAPE) {
    this.focusManager.removeHelp();
    return RESOLVED;
  }

  if (ev.keyCode === KeyEvent.DOM_VK_UP) {
    return this._handleUpArrow();
  }

  if (ev.keyCode === KeyEvent.DOM_VK_DOWN) {
    return this._handleDownArrow();
  }

  if (ev.keyCode === KeyEvent.DOM_VK_RETURN) {
    return this._handleReturn();
  }

  if (ev.keyCode === KeyEvent.DOM_VK_TAB && !ev.shiftKey) {
    return this._handleTab(ev);
  }

  if (this._previousValue === this.element.value) {
    return RESOLVED;
  }

  this._scrollingThroughHistory = false;
  this._caretChange = Caret.NO_CHANGE;

  this._completed = this.requisition.update(this.element.value);
  this._previousValue = this.element.value;

  return this._completed.then(function(updated) {
    // Abort UI changes if this UI update has been overtaken
    if (updated) {
      this._choice = null;
      this.textChanged();
      this.onChoiceChange({ choice: this._choice });
    }
  }.bind(this));
};

/**
 * See also _handleDownArrow for some symmetry
 */
Inputter.prototype._handleUpArrow = function() {
  if (this.tooltip && this.tooltip.isMenuShowing) {
    this.changeChoice(-1);
    return RESOLVED;
  }

  if (this.element.value === '' || this._scrollingThroughHistory) {
    this._scrollingThroughHistory = true;
    return this.requisition.update(this.history.backward()).then(function(updated) {
      this.textChanged();
      return updated;
    }.bind(this));
  }

  // If the user is on a valid value, then we increment the value, but if
  // they've typed something that's not right we page through predictions
  if (this.assignment.getStatus() === Status.VALID) {
    return this.requisition.increment(this.assignment).then(function() {
      // See notes on focusManager.onInputChange in onKeyDown
      this.textChanged();
      if (this.focusManager) {
        this.focusManager.onInputChange();
      }
    }.bind(this));
  }

  this.changeChoice(-1);
  return RESOLVED;
};

/**
 * See also _handleUpArrow for some symmetry
 */
Inputter.prototype._handleDownArrow = function() {
  if (this.tooltip && this.tooltip.isMenuShowing) {
    this.changeChoice(+1);
    return RESOLVED;
  }

  if (this.element.value === '' || this._scrollingThroughHistory) {
    this._scrollingThroughHistory = true;
    return this.requisition.update(this.history.forward()).then(function(updated) {
      this.textChanged();
      return updated;
    }.bind(this));
  }

  // See notes above for the UP key
  if (this.assignment.getStatus() === Status.VALID) {
    return this.requisition.decrement(this.assignment).then(function() {
      // See notes on focusManager.onInputChange in onKeyDown
      this.textChanged();
      if (this.focusManager) {
        this.focusManager.onInputChange();
      }
    }.bind(this));
  }

  this.changeChoice(+1);
  return RESOLVED;
};

/**
 * RETURN checks status and might exec
 */
Inputter.prototype._handleReturn = function() {
  // Deny RETURN unless the command might work
  if (this.requisition.status === Status.VALID) {
    this._scrollingThroughHistory = false;
    this.history.add(this.element.value);

    return this.requisition.exec().then(function() {
      this.textChanged();
    }.bind(this));
  }

  // If we can't execute the command, but there is a menu choice to use
  // then use it.
  if (!this.tooltip.selectChoice()) {
    this.focusManager.setError(true);
  }

  this._choice = null;
  return RESOLVED;
};

/**
 * Warning: We get TAB events for more than just the user pressing TAB in our
 * input element.
 */
Inputter.prototype._handleTab = function(ev) {
  // Being able to complete 'nothing' is OK if there is some context, but
  // when there is nothing on the command line it just looks bizarre.
  var hasContents = (this.element.value.length > 0);

  // If the TAB keypress took the cursor from another field to this one,
  // then they get the keydown/keypress, and we get the keyup. In this
  // case we don't want to do any completion.
  // If the time of the keydown/keypress of TAB was close (i.e. within
  // 1 second) to the time of the keyup then we assume that we got them
  // both, and do the completion.
  if (hasContents && this.lastTabDownAt + 1000 > ev.timeStamp) {
    // It's possible for TAB to not change the input, in which case the
    // textChanged event will not fire, and the caret move will not be
    // processed. So we check that this is done first
    this._caretChange = Caret.TO_ARG_END;
    var inputState = this.getInputState();
    this._processCaretChange(inputState);

    if (this._choice == null) {
      this._choice = 0;
    }

    // The changes made by complete may happen asynchronously, so after the
    // the call to complete() we should avoid making changes before the end
    // of the event loop
    this._completed = this.requisition.complete(inputState.cursor,
                                                this._choice);
    this._previousValue = this.element.value;
  }
  this.lastTabDownAt = 0;
  this._scrollingThroughHistory = false;

  return this._completed.then(function(updated) {
    // Abort UI changes if this UI update has been overtaken
    if (updated) {
      this.textChanged();
      this._choice = null;
      this.onChoiceChange({ choice: this._choice });
    }
  }.bind(this));
};

/**
 * Used by onKeyUp for UP/DOWN to change the current choice from an options
 * menu.
 */
Inputter.prototype.changeChoice = function(amount) {
  if (this._choice == null) {
    this._choice = 0;
  }
  // There's an annoying up is down thing here, the menu is presented
  // with the zeroth index at the top working down, so the UP arrow needs
  // pick the choice below because we're working down
  this._choice += amount;
  this.onChoiceChange({ choice: this._choice });
};

/**
 * Pull together an input object, which may include XUL hacks
 */
Inputter.prototype.getInputState = function() {
  var input = {
    typed: this.element.value,
    cursor: {
      start: this.element.selectionStart,
      end: this.element.selectionEnd
    }
  };

  // Workaround for potential XUL bug 676520 where textbox gives incorrect
  // values for its content
  if (input.typed == null) {
    input = { typed: '', cursor: { start: 0, end: 0 } };
  }

  return input;
};

exports.Inputter = Inputter;
