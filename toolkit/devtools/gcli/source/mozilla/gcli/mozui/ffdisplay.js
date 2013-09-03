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

var Inputter = require('./inputter').Inputter;
var Completer = require('./completer').Completer;
var Tooltip = require('./tooltip').Tooltip;
var FocusManager = require('../ui/focus').FocusManager;

var Requisition = require('../cli').Requisition;

var cli = require('../cli');
var jstype = require('../types/javascript');
var nodetype = require('../types/node');
var resource = require('../types/resource');
var intro = require('../ui/intro');

var CommandOutputManager = require('../commands/commands').CommandOutputManager;

/**
 * Handy utility to inject the content document (i.e. for the viewed page,
 * not for chrome) into the various components.
 */
function setContentDocument(document) {
  if (document) {
    nodetype.setDocument(document);
    resource.setDocument(document);
  }
  else {
    resource.unsetDocument();
    nodetype.unsetDocument();
    jstype.unsetGlobalObject();
  }
}

/**
 * FFDisplay is responsible for generating the UI for GCLI, this implementation
 * is a special case for use inside Firefox
 * @param options A configuration object containing the following:
 * - contentDocument (optional)
 * - chromeDocument
 * - hintElement
 * - inputElement
 * - completeElement
 * - backgroundElement
 * - outputDocument
 * - consoleWrap (optional)
 * - eval (optional)
 * - environment
 * - chromeWindow
 * - commandOutputManager (optional)
 */
function FFDisplay(options) {
  if (options.eval) {
    cli.setEvalFunction(options.eval);
  }
  setContentDocument(options.contentDocument);

  this.commandOutputManager = options.commandOutputManager;
  if (this.commandOutputManager == null) {
    this.commandOutputManager = new CommandOutputManager();
  }

  this.onOutput = this.commandOutputManager.onOutput;
  this.requisition = new Requisition({
    environment: options.environment,
    document: options.outputDocument,
    commandOutputManager: this.commandOutputManager
  });

  this.focusManager = new FocusManager(options.chromeDocument);
  this.onVisibilityChange = this.focusManager.onVisibilityChange;

  this.inputter = new Inputter(options, {
    requisition: this.requisition,
    focusManager: this.focusManager,
    element: options.inputElement
  });

  this.completer = new Completer({
    requisition: this.requisition,
    inputter: this.inputter,
    backgroundElement: options.backgroundElement,
    element: options.completeElement
  });

  this.tooltip = new Tooltip(options, {
    requisition: this.requisition,
    focusManager: this.focusManager,
    inputter: this.inputter,
    element: options.hintElement
  });

  this.inputter.tooltip = this.tooltip;

  if (options.consoleWrap) {
    this.resizer = this.resizer.bind(this);

    this.consoleWrap = options.consoleWrap;
    var win = options.consoleWrap.ownerDocument.defaultView;
    win.addEventListener('resize', this.resizer, false);
  }

  this.options = options;
}

/**
 * The main Display calls this as part of startup since it registers listeners
 * for output first. The firefox display can't do this, so it has to be a
 * separate method
 */
FFDisplay.prototype.maybeShowIntro = function() {
  intro.maybeShowIntro(this.commandOutputManager,
                       this.requisition.conversionContext);
};

/**
 * Called when the page to which we're attached changes
 * @params options Object with the following properties:
 * - contentDocument: Points to the page that we should now work against
 */
FFDisplay.prototype.reattach = function(options) {
  setContentDocument(options.contentDocument);
};

/**
 * Avoid memory leaks
 */
FFDisplay.prototype.destroy = function() {
  if (this.consoleWrap) {
    var win = this.options.consoleWrap.ownerDocument.defaultView;
    win.removeEventListener('resize', this.resizer, false);
  }

  this.tooltip.destroy();
  this.completer.destroy();
  this.inputter.destroy();
  this.focusManager.destroy();

  this.requisition.destroy();

  setContentDocument(null);
  cli.unsetEvalFunction();

  delete this.options;

  // We could also delete the following objects if we have hard-to-track-down
  // memory leaks, as a belt-and-braces approach, however this prevents our
  // DOM node hunter script from looking in all the nooks and crannies, so it's
  // better if we can be leak-free without deleting them:
  // - consoleWrap, resizer, tooltip, completer, inputter,
  // - focusManager, onVisibilityChange, requisition, commandOutputManager
};

/**
 * Called on chrome window resize, or on divider slide
 */
FFDisplay.prototype.resizer = function() {
  // Bug 705109: There are several numbers hard-coded in this function.
  // This is simpler than calculating them, but error-prone when the UI setup,
  // the styling or display settings change.

  var parentRect = this.options.consoleWrap.getBoundingClientRect();
  // Magic number: 64 is the size of the toolbar above the output area
  var parentHeight = parentRect.bottom - parentRect.top - 64;

  // Magic number: 100 is the size at which we decide the hints are too small
  // to be useful, so we hide them
  if (parentHeight < 100) {
    this.options.hintElement.classList.add('gcliterm-hint-nospace');
  }
  else {
    this.options.hintElement.classList.remove('gcliterm-hint-nospace');
    this.options.hintElement.style.overflowY = null;
    this.options.hintElement.style.borderBottomColor = 'white';
  }

  // We also try to make the max-width of any GCLI elements so they don't
  // extend outside the scroll area.
  var doc = this.options.hintElement.ownerDocument;

  var outputNode = this.options.hintElement.parentNode.parentNode.children[1];
  var listItems = outputNode.getElementsByClassName('hud-msg-node');

  // This is an top-side estimate. We could try to calculate it, maybe using
  // something along these lines http://www.alexandre-gomes.com/?p=115 However
  // experience has shown this to be hard to get to work reliably
  // Also we don't need to be precise. If we use a number that is too big then
  // the only down-side is too great a right margin
  var scrollbarWidth = 20;

  if (listItems.length > 0) {
    var parentWidth = outputNode.getBoundingClientRect().width - scrollbarWidth;
    var otherWidth;
    var body;

    for (var i = 0; i < listItems.length; ++i) {
      var listItem = listItems[i];
      // a.k.a 'var otherWidth = 132'
      otherWidth = 0;
      body = null;

      for (var j = 0; j < listItem.children.length; j++) {
        var child = listItem.children[j];

        if (child.classList.contains('gcliterm-msg-body')) {
          body = child.children[0];
        }
        else {
          otherWidth += child.getBoundingClientRect().width;
        }

        var styles = doc.defaultView.getComputedStyle(child, null);
        otherWidth += parseInt(styles.borderLeftWidth, 10) +
                      parseInt(styles.borderRightWidth, 10) +
                      parseInt(styles.paddingLeft, 10) +
                      parseInt(styles.paddingRight, 10) +
                      parseInt(styles.marginLeft, 10) +
                      parseInt(styles.marginRight, 10);
      }

      if (body) {
        body.style.width = (parentWidth - otherWidth) + 'px';
      }
    }
  }
};

exports.FFDisplay = FFDisplay;
