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
var domtemplate = require('../util/domtemplate');
var KeyEvent = require('../util/util').KeyEvent;
var host = require('../util/host');

var languages = require('../languages/languages');
var History = require('./history').History;

var FocusManager = require('./focus').FocusManager;

var RESOLVED = promise.resolve(undefined);

/**
 * Shared promises for loading resource files
 */
var resourcesPromise;

/**
 * Asynchronous construction. Use Terminal.create();
 * @private
 */
function Terminal() {
  throw new Error('Use Terminal.create().then(...) rather than new Terminal()');
}

/**
 * A wrapper to take care of the functions concerning an input element
 * @param components Object that links to other UI components. GCLI provided:
 * - requisition
 * - document
 */
Terminal.create = function(options) {
  if (resourcesPromise == null) {
    resourcesPromise = promise.all([
      host.staticRequire(module, './terminal.css'),
      host.staticRequire(module, './terminal.html')
    ]);
  }
  return resourcesPromise.then(function(resources) {
    var terminal = Object.create(Terminal.prototype);
    return terminal._init(options, resources[0], resources[1]);
  });
};

/**
 * Asynchronous construction. Use Terminal.create();
 * @private
 */
Terminal.prototype._init = function(options, terminalCss, terminalHtml) {
  this.document = options.document || document;
  this.options = options;

  this.focusManager = new FocusManager(this.document);
  this.onInputChange = util.createEvent('Terminal.onInputChange');

  // Configure the UI
  this.rootElement = this.document.getElementById('gcli-root');
  if (!this.rootElement) {
    throw new Error('Missing element, id=gcli-root');
  }
  this.rootElement.terminal = this;

  // terminal.html contains sub-templates which we detach for later processing
  var template = util.toDom(this.document, terminalHtml);

  // JSDom appears to have a broken parentElement, so this is a workaround
  // this.tooltipTemplate = template.querySelector('.gcli-tt');
  // this.tooltipTemplate.parentElement.removeChild(this.tooltipTemplate);
  var tooltipParent = template.querySelector('.gcli-tooltip');
  this.tooltipTemplate = tooltipParent.children[0];
  tooltipParent.removeChild(this.tooltipTemplate);

  // this.completerTemplate = template.querySelector('.gcli-in-complete > div');
  // this.completerTemplate.parentElement.removeChild(this.completerTemplate);
  var completerParent = template.querySelector('.gcli-in-complete');
  this.completerTemplate = completerParent.children[0];
  completerParent.removeChild(this.completerTemplate);
  // We want the spans to line up without the spaces in the template
  util.removeWhitespace(this.completerTemplate, true);

  // Now we've detached the sub-templates, load what is left
  // The following elements are stored into 'this' by this template process:
  // displayElement, panelElement, tooltipElement,
  // inputElement, completeElement, promptElement
  domtemplate.template(template, this, { stack: 'terminal.html' });
  while (template.hasChildNodes()) {
    this.rootElement.appendChild(template.firstChild);
  }

  if (terminalCss != null) {
    this.style = util.importCss(terminalCss, this.document, 'gcli-tooltip');
  }

  this.tooltipElement.classList.add('gcli-panel-hide');

  // Firefox doesn't autofocus with dynamically added elements (Bug 662496)
  this.inputElement.focus();

  // Used to distinguish focus from TAB in CLI. See onKeyUp()
  this.lastTabDownAt = 0;

  // Setup History
  this.history = new History();
  this._scrollingThroughHistory = false;

  // Initially an asynchronous completion isn't in-progress
  this._completed = RESOLVED;

  // Avoid updating when the keyUp results in no change
  this._previousValue = undefined;

  // We cache the fields we create so we can destroy them later
  this.fields = [];

  // Bind handlers
  this.focus = this.focus.bind(this);
  this.onKeyDown = this.onKeyDown.bind(this);
  this.onKeyUp = this.onKeyUp.bind(this);
  this.onMouseUp = this.onMouseUp.bind(this);
  this.onOutput = this.onOutput.bind(this);

  this.rootElement.addEventListener('click', this.focus, false);

  // Ensure that TAB/UP/DOWN isn't handled by the browser
  this.inputElement.addEventListener('keydown', this.onKeyDown, false);
  this.inputElement.addEventListener('keyup', this.onKeyUp, false);

  // Cursor position affects hint severity
  this.inputElement.addEventListener('mouseup', this.onMouseUp, false);

  this.focusManager.onVisibilityChange.add(this.visibilityChanged, this);
  this.focusManager.addMonitoredElement(this.tooltipElement, 'tooltip');
  this.focusManager.addMonitoredElement(this.inputElement, 'input');

  this.onInputChange.add(this.updateCompletion, this);

  host.script.onOutput.add(this.onOutput);

  // Use the default language
  return this.switchLanguage(null).then(function() {
    return this;
  }.bind(this));
};

/**
 * Avoid memory leaks
 */
Terminal.prototype.destroy = function() {
  this.focusManager.removeMonitoredElement(this.inputElement, 'input');
  this.focusManager.removeMonitoredElement(this.tooltipElement, 'tooltip');
  this.focusManager.onVisibilityChange.remove(this.visibilityChanged, this);

  this.inputElement.removeEventListener('mouseup', this.onMouseUp, false);
  this.inputElement.removeEventListener('keydown', this.onKeyDown, false);
  this.inputElement.removeEventListener('keyup', this.onKeyUp, false);
  this.rootElement.removeEventListener('click', this.focus, false);

  this.language.destroy();
  this.history.destroy();
  this.focusManager.destroy();

  if (this.style) {
    this.style.parentNode.removeChild(this.style);
    this.style = undefined;
  }

  this.field.onFieldChange.remove(this.fieldChanged, this);
  this.field.destroy();

  this.onInputChange.remove(this.updateCompletion, this);

  // Remove the output elements so they free the event handers
  util.clearElement(this.displayElement);

  this.focus = undefined;
  this.onMouseUp = undefined;
  this.onKeyDown = undefined;
  this.onKeyUp = undefined;

  this.rootElement = undefined;
  this.inputElement = undefined;
  this.promptElement = undefined;
  this.completeElement = undefined;
  this.tooltipElement = undefined;
  this.panelElement = undefined;
  this.displayElement = undefined;

  this.completerTemplate = undefined;
  this.tooltipTemplate = undefined;

  this.errorEle = undefined;
  this.descriptionEle = undefined;

  this.document = undefined;
};

/**
 * Use an alternative language
 */
Terminal.prototype.switchLanguage = function(name) {
  if (this.language != null) {
    this.language.destroy();
  }

  return languages.createLanguage(name, this).then(function(language) {
    this._updateLanguage(language);
  }.bind(this));
};

/**
 * Temporarily use an alternative language
 */
Terminal.prototype.pushLanguage = function(name) {
  return languages.createLanguage(name, this).then(function(language) {
    this.origLanguage = this.language;
    this._updateLanguage(language);
  }.bind(this));
};

/**
 * Return to use the original language
 */
Terminal.prototype.popLanguage = function() {
  if (this.origLanguage == null) {
    return RESOLVED;
  }

  this._updateLanguage(this.origLanguage);
  this.origLanguage = undefined;

  return RESOLVED;
};

/**
 * Internal helper to make sure everything knows about the new language
 */
Terminal.prototype._updateLanguage = function(language) {
  this.language = language;

  if (this.language.proportionalFonts) {
    this.topElement.classList.remove('gcli-in-script');
  }
  else {
    this.topElement.classList.add('gcli-in-script');
  }

  this.language.updateHints();
  this.updateCompletion();
  this.promptElement.innerHTML = this.language.prompt;
};

/**
 * Sometimes the environment provides asynchronous output, we display it here
 */
Terminal.prototype.onOutput = function(ev) {
  console.log('onOutput', ev);

  var rowoutEle = this.document.createElement('pre');
  rowoutEle.classList.add('gcli-row-out');
  rowoutEle.classList.add('gcli-row-script');
  rowoutEle.setAttribute('aria-live', 'assertive');

  var output = '         // ';
  if (ev.level === 'warn') {
    output += '!';
  }
  else if (ev.level === 'error') {
    output += '✖';
  }
  else {
    output += '→';
  }

  output += ' ' + ev.arguments.map(function(arg) {
    return arg;
  }).join(',');

  rowoutEle.innerHTML = output;

  this.addElement(rowoutEle);
};

/**
 * Handler for the input-element.onMouseUp event
 */
Terminal.prototype.onMouseUp = function(ev) {
  this.language.caretMoved(this.inputElement.selectionStart);
};

/**
 * Set the input field to a value, for external use.
 * It does not execute the input or affect the history.
 * This function should not be called internally, by Terminal and never as a
 * result of a keyboard event on this.inputElement or bug 676520 could be
 * triggered.
 */
Terminal.prototype.setInput = function(str) {
  this._scrollingThroughHistory = false;
  return this._setInput(str);
};

/**
 * @private Internal version of setInput
 */
Terminal.prototype._setInput = function(str) {
  this.inputElement.value = str;
  this._previousValue = this.inputElement.value;

  this._completed = this.language.handleInput(str);
  return this._completed;
};

/**
 * Focus the input element
 */
Terminal.prototype.focus = function() {
  this.inputElement.focus();
  this.language.caretMoved(this.inputElement.selectionStart);
};

/**
 * Ensure certain keys (arrows, tab, etc) that we would like to handle
 * are not handled by the browser
 */
Terminal.prototype.onKeyDown = function(ev) {
  if (ev.keyCode === KeyEvent.DOM_VK_UP ||
      ev.keyCode === KeyEvent.DOM_VK_DOWN) {
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

  if (ev.keyCode === KeyEvent.DOM_VK_BACK_SPACE &&
      this.inputElement.value === '') {
    return this.popLanguage();
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
        this.inputElement.blur();
      }
    }
  }
};

/**
 * Handler for use with DOM events, which just calls the promise enabled
 * handleKeyUp function but checks the exit state of the promise so we know
 * if something went wrong.
 */
Terminal.prototype.onKeyUp = function(ev) {
  this.handleKeyUp(ev).then(null, util.errorHandler);
};

/**
 * The main keyboard processing loop
 * @return A promise that resolves (to undefined) when the actions kicked off
 * by this handler are completed.
 */
Terminal.prototype.handleKeyUp = function(ev) {
  if (this.focusManager && ev.keyCode === KeyEvent.DOM_VK_F1) {
    this.focusManager.helpRequest();
    return RESOLVED;
  }

  if (this.focusManager && ev.keyCode === KeyEvent.DOM_VK_ESCAPE) {
    if (this.focusManager.isTooltipVisible ||
        this.focusManager.isOutputVisible) {
      this.focusManager.removeHelp();
    }
    else if (this.inputElement.value === '') {
      return this.popLanguage();
    }
    return RESOLVED;
  }

  if (ev.keyCode === KeyEvent.DOM_VK_UP) {
    if (this.isMenuShowing) {
      return this.incrementChoice();
    }

    if (this.inputElement.value === '' || this._scrollingThroughHistory) {
      this._scrollingThroughHistory = true;
      return this._setInput(this.history.backward());
    }

    return this.language.handleUpArrow().then(function(handled) {
      if (!handled) {
        return this.incrementChoice();
      }
    }.bind(this));
  }

  if (ev.keyCode === KeyEvent.DOM_VK_DOWN) {
    if (this.isMenuShowing) {
      return this.decrementChoice();
    }

    if (this.inputElement.value === '' || this._scrollingThroughHistory) {
      this._scrollingThroughHistory = true;
      return this._setInput(this.history.forward());
    }

    return this.language.handleDownArrow().then(function(handled) {
      if (!handled) {
        return this.decrementChoice();
      }
    }.bind(this));
  }

  if (ev.keyCode === KeyEvent.DOM_VK_RETURN) {
    var input = this.inputElement.value;
    return this.language.handleReturn(input).then(function(handled) {
      if (!handled) {
        this._scrollingThroughHistory = false;

        if (!this.selectChoice()) {
          this.focusManager.setError(true);
        }
        else {
          return this.popLanguage();
        }
      }
      else {
        return this.popLanguage();
      }
    }.bind(this));
  }

  if (ev.keyCode === KeyEvent.DOM_VK_TAB && !ev.shiftKey) {
    // Being able to complete 'nothing' is OK if there is some context, but
    // when there is nothing on the command line it just looks bizarre.
    var hasContents = (this.inputElement.value.length > 0);

    // If the TAB keypress took the cursor from another field to this one,
    // then they get the keydown/keypress, and we get the keyup. In this
    // case we don't want to do any completion.
    // If the time of the keydown/keypress of TAB was close (i.e. within
    // 1 second) to the time of the keyup then we assume that we got them
    // both, and do the completion.
    if (hasContents && this.lastTabDownAt + 1000 > ev.timeStamp) {
      this._completed = this.language.handleTab();
    }
    else {
      this._completed = RESOLVED;
    }

    this.lastTabDownAt = 0;
    this._scrollingThroughHistory = false;

    return this._completed;
  }

  if (this._previousValue === this.inputElement.value) {
    return RESOLVED;
  }

  var value = this.inputElement.value;
  this._scrollingThroughHistory = false;
  this._previousValue = this.inputElement.value;

  this._completed = this.language.handleInput(value);
  return this._completed;
};

/**
 * What is the index of the currently highlighted option?
 */
Terminal.prototype.getChoiceIndex = function() {
  return this.field && this.field.menu ? this.field.menu.getChoiceIndex() : 0;
};

/**
 * Don't show any menu options
 */
Terminal.prototype.unsetChoice = function() {
  if (this.field && this.field.menu) {
    this.field.menu.unsetChoice();
  }
  return this.updateCompletion();
};

/**
 * Select the previous option in a list of choices
 */
Terminal.prototype.incrementChoice = function() {
  if (this.field && this.field.menu) {
    this.field.menu.incrementChoice();
  }
  return this.updateCompletion();
};

/**
 * Select the next option in a list of choices
 */
Terminal.prototype.decrementChoice = function() {
  if (this.field && this.field.menu) {
    this.field.menu.decrementChoice();
  }
  return this.updateCompletion();
};

/**
 * Pull together an input object, which may include XUL hacks
 */
Terminal.prototype.getInputState = function() {
  var input = {
    typed: this.inputElement.value,
    cursor: {
      start: this.inputElement.selectionStart,
      end: this.inputElement.selectionEnd
    }
  };

  // Workaround for potential XUL bug 676520 where textbox gives incorrect
  // values for its content
  if (input.typed == null) {
    input = { typed: '', cursor: { start: 0, end: 0 } };
  }

  return input;
};

/**
 * Bring the completion element up to date with what the language says
 */
Terminal.prototype.updateCompletion = function() {
  return this.language.getCompleterTemplateData().then(function(data) {
    var template = this.completerTemplate.cloneNode(true);
    domtemplate.template(template, data, { stack: 'terminal.html#completer' });

    util.clearElement(this.completeElement);
    while (template.hasChildNodes()) {
      this.completeElement.appendChild(template.firstChild);
    }
  }.bind(this));
};

/**
 * The terminal acts on UP/DOWN if there is a menu showing
 */
Object.defineProperty(Terminal.prototype, 'isMenuShowing', {
  get: function() {
    return this.focusManager.isTooltipVisible &&
           this.field != null &&
           this.field.menu != null;
  },
  enumerable: true
});

/**
 * Allow the terminal to use RETURN to chose the current menu item when
 * it can't execute the command line
 * @return true if there was a selection to use, false otherwise
 */
Terminal.prototype.selectChoice = function(ev) {
  if (this.field && this.field.selectChoice) {
    return this.field.selectChoice();
  }
  return false;
};

/**
 * Called by the onFieldChange event on the current Field
 */
Terminal.prototype.fieldChanged = function(ev) {
  this.language.fieldChanged(ev);

  // Nasty hack, the terminal won't know about the text change yet, so it will
  // get it's calculations wrong. We need to wait until the current set of
  // changes has had a chance to propagate
  this.document.defaultView.setTimeout(function() {
    this.focus();
  }.bind(this), 10);
};

/**
 * Tweak CSS to show/hide the output
 */
Terminal.prototype.visibilityChanged = function(ev) {
  if (!this.panelElement) {
    return;
  }

  if (ev.tooltipVisible) {
    this.tooltipElement.classList.remove('gcli-panel-hide');
  }
  else {
    this.tooltipElement.classList.add('gcli-panel-hide');
  }
  this.scrollToBottom();
};

/**
 * For language to add elements to the output
 */
Terminal.prototype.addElement = function(element) {
  this.displayElement.insertBefore(element, this.topElement);
};

/**
 * Clear the added added output
 */
Terminal.prototype.clear = function() {
  while (this.displayElement.hasChildNodes()) {
    if (this.displayElement.firstChild === this.topElement) {
      break;
    }
    this.displayElement.removeChild(this.displayElement.firstChild);
  }
};

/**
 * Scroll the output area down to make the input visible
 */
Terminal.prototype.scrollToBottom = function() {
  // We need to see the output of the latest command entered
  // Certain browsers have a bug such that scrollHeight is too small
  // when content does not fill the client area of the element
  var scrollHeight = Math.max(this.displayElement.scrollHeight,
                              this.displayElement.clientHeight);
  this.displayElement.scrollTop =
                      scrollHeight - this.displayElement.clientHeight;
};

exports.Terminal = Terminal;
