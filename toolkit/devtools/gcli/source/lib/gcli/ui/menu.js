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
var l10n = require('../util/l10n');
var domtemplate = require('../util/domtemplate');
var host = require('../util/host');

/**
 * Shared promises for loading resource files
 */
var menuCssPromise;
var menuHtmlPromise;

/**
 * Menu is a display of the commands that are possible given the state of a
 * requisition.
 * @param options A way to customize the menu display.
 * - document: The document to use in creating widgets
 * - maxPredictions (default=8): The maximum predictions to show at one time
 *     If more are requested, a message will be displayed asking the user to
 *     continue typing to narrow the list of options
 */
function Menu(options) {
  options = options || {};
  this.document = options.document || document;
  this.maxPredictions = options.maxPredictions || 8;

  // Keep track of any highlighted items
  this._choice = null;

  // FF can be really hard to debug if doc is null, so we check early on
  if (!this.document) {
    throw new Error('No document');
  }

  this.element =  util.createElement(this.document, 'div');
  this.element.classList.add('gcli-menu');

  if (menuCssPromise == null) {
    menuCssPromise = host.staticRequire(module, './menu.css');
  }
  menuCssPromise.then(function(menuCss) {
    // Pull the HTML into the DOM, but don't add it to the document
    if (menuCss != null) {
      util.importCss(menuCss, this.document, 'gcli-menu');
    }
  }.bind(this));

  this.templateOptions = { blankNullUndefined: true, stack: 'menu.html' };
  if (menuHtmlPromise == null) {
    menuHtmlPromise = host.staticRequire(module, './menu.html');
  }
  menuHtmlPromise.then(function(menuHtml) {
    this.template = util.toDom(this.document, menuHtml);
  }.bind(this));

  // Contains the items that should be displayed
  this.items = [];

  this.onItemClick = util.createEvent('Menu.onItemClick');
}

/**
 * Allow the template engine to get at localization strings
 */
Menu.prototype.l10n = l10n.propertyLookup;

/**
 * Avoid memory leaks
 */
Menu.prototype.destroy = function() {
  this.element = undefined;
  this.template = undefined;
  this.document = undefined;
  this.items = undefined;
};

/**
 * The default is to do nothing when someone clicks on the menu.
 * This is called from template.html
 * @param ev The click event from the browser
 */
Menu.prototype.onItemClickInternal = function(ev) {
  var name = ev.currentTarget.querySelector('.gcli-menu-name').textContent;
  this.onItemClick({ name: name });
};

/**
 * What is the currently selected item?
 */
Object.defineProperty(Menu.prototype, 'selected', {
  get: function() {
    var item = this.element.querySelector('.gcli-menu-highlight .gcli-menu-name');
    if (!item) {
      return null;
    }
    return item.textContent;
  },
  enumerable: true
});

/**
 * Display a number of items in the menu (or hide the menu if there is nothing
 * to display)
 * @param items The items to show in the menu
 * @param match Matching text to highlight in the output
 */
Menu.prototype.show = function(items, match) {
  // If the HTML hasn't loaded yet then just don't show a menu
  if (this.template == null) {
    return;
  }

  this.items = items.filter(function(item) {
    return item.hidden === undefined || item.hidden !== true;
  }.bind(this));

  if (match) {
    this.items = this.items.map(function(item) {
      return getHighlightingProxy(item, match, this.template.ownerDocument);
    }.bind(this));
  }

  if (this.items.length === 0) {
    this.element.style.display = 'none';
    return;
  }

  if (this.items.length >= this.maxPredictions) {
    this.items.splice(-1);
    this.hasMore = true;
  }
  else {
    this.hasMore = false;
  }

  var options = this.template.cloneNode(true);
  domtemplate.template(options, this, this.templateOptions);

  util.clearElement(this.element);
  this.element.appendChild(options);

  this.element.style.display = 'block';
};

/**
 * Create a proxy around an item that highlights matching text
 */
function getHighlightingProxy(item, match, document) {
  if (typeof Proxy === 'undefined') {
    return item;
  }
  return Proxy.create({
    get: function(rcvr, name) {
      var value = item[name];
      if (name !== 'name') {
        return value;
      }

      var startMatch = value.indexOf(match);
      if (startMatch === -1) {
        return value;
      }

      var before = value.substr(0, startMatch);
      var after = value.substr(startMatch + match.length);
      var parent = util.createElement(document, 'span');
      parent.appendChild(document.createTextNode(before));
      var highlight = util.createElement(document, 'span');
      highlight.classList.add('gcli-menu-typed');
      highlight.appendChild(document.createTextNode(match));
      parent.appendChild(highlight);
      parent.appendChild(document.createTextNode(after));
      return parent;
    }
  });
}

/**
 * @return The current choice index
 */
Menu.prototype.getChoiceIndex = function() {
  return this._choice == null ? 0 : this._choice;
};

/**
 * Highlight the next option
 */
Menu.prototype.incrementChoice = function() {
  if (this._choice == null) {
    this._choice = 0;
  }

  // There's an annoying up is down thing here, the menu is presented
  // with the zeroth index at the top working down, so the UP arrow needs
  // pick the choice below because we're working down
  this._choice--;
  this._updateHighlight();
};

/**
 * Highlight the previous option
 */
Menu.prototype.decrementChoice = function() {
  if (this._choice == null) {
    this._choice = 0;
  }

  // See incrementChoice
  this._choice++;
  this._updateHighlight();
};

/**
 * Highlight nothing
 */
Menu.prototype.unsetChoice = function() {
  this._choice = null;
  this._updateHighlight();
};

/**
 * Internal option to update the currently highlighted option
 */
Menu.prototype._updateHighlight = function() {
  var nodes = this.element.querySelectorAll('.gcli-menu-option');
  for (var i = 0; i < nodes.length; i++) {
    nodes[i].classList.remove('gcli-menu-highlight');
  }

  if (this._choice == null || nodes.length === 0) {
    return;
  }

  var index = this._choice % nodes.length;
  if (index < 0) {
    index = nodes.length + index;
  }

  nodes.item(index).classList.add('gcli-menu-highlight');
};

/**
 * Hide the menu
 */
Menu.prototype.hide = function() {
  this.element.style.display = 'none';
};

/**
 * Change how much vertical space this menu can take up
 */
Menu.prototype.setMaxHeight = function(height) {
  this.element.style.maxHeight = height + 'px';
};

exports.Menu = Menu;
