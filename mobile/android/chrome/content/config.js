/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var {classes: Cc, interfaces: Ci, manager: Cm, utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");

const VKB_ENTER_KEY = 13;   // User press of VKB enter key
const INITIAL_PAGE_DELAY = 500;   // Initial pause on program start for scroll alignment
const PREFS_BUFFER_MAX = 30;   // Max prefs buffer size for getPrefsBuffer()
const PAGE_SCROLL_TRIGGER = 200;     // Triggers additional getPrefsBuffer() on user scroll-to-bottom
const FILTER_CHANGE_TRIGGER = 200;     // Delay between responses to filterInput changes
const INNERHTML_VALUE_DELAY = 100;    // Delay before providing prefs innerHTML value

var gStringBundle = Services.strings.createBundle("chrome://browser/locale/config.properties");
var gClipboardHelper = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);


/* ============================== NewPrefDialog ==============================
 *
 * New Preference Dialog Object and methods
 *
 * Implements User Interfaces for creation of a single(new) Preference setting
 *
 */
var NewPrefDialog = {

  _prefsShield: null,

  _newPrefsDialog: null,
  _newPrefItem: null,
  _prefNameInputElt: null,
  _prefTypeSelectElt: null,

  _booleanValue: null,
  _booleanToggle: null,
  _stringValue: null,
  _intValue: null,

  _positiveButton: null,

  get type() {
    return this._prefTypeSelectElt.value;
  },

  set type(aType) {
    this._prefTypeSelectElt.value = aType;
    switch(this._prefTypeSelectElt.value) {
      case "boolean":
        this._prefTypeSelectElt.selectedIndex = 0;
        break;
      case "string":
        this._prefTypeSelectElt.selectedIndex = 1;
        break;
      case "int":
        this._prefTypeSelectElt.selectedIndex = 2;
        break;
    }

    this._newPrefItem.setAttribute("typestyle", aType);
  },

  // Init the NewPrefDialog
  init: function AC_init() {
    this._prefsShield = document.getElementById("prefs-shield");

    this._newPrefsDialog = document.getElementById("new-pref-container");
    this._newPrefItem = document.getElementById("new-pref-item");
    this._prefNameInputElt = document.getElementById("new-pref-name");
    this._prefTypeSelectElt = document.getElementById("new-pref-type");

    this._booleanValue = document.getElementById("new-pref-value-boolean");
    this._stringValue = document.getElementById("new-pref-value-string");
    this._intValue = document.getElementById("new-pref-value-int");

    this._positiveButton = document.getElementById("positive-button");
  },

  // Called to update positive button to display text ("Create"/"Change), and enabled/disabled status
  // As new pref name is initially displayed, re-focused, or modifed during user input
  _updatePositiveButton: function AC_updatePositiveButton(aPrefName) {
    this._positiveButton.textContent = gStringBundle.GetStringFromName("newPref.createButton");
    this._positiveButton.setAttribute("disabled", true);
    if (aPrefName == "") {
      return;
    }

    // If item already in list, it's being changed, else added
    let item = AboutConfig._list.filter(i => { return i.name == aPrefName });
    if (item.length) {
      this._positiveButton.textContent = gStringBundle.GetStringFromName("newPref.changeButton");
    } else {
      this._positiveButton.removeAttribute("disabled");
    }
  },

  // When we want to cancel/hide an existing, or show a new pref dialog
  toggleShowHide: function AC_toggleShowHide() {
    if (this._newPrefsDialog.classList.contains("show")) {
      this.hide();
    } else {
      this._show();
    }
  },

  // When we want to show the new pref dialog / shield the prefs list
  _show: function AC_show() {
    this._newPrefsDialog.classList.add("show");
    this._prefsShield.setAttribute("shown", true);

    // Initial default field values
    this._prefNameInputElt.value = "";
    this._updatePositiveButton(this._prefNameInputElt.value);

    this.type = "boolean";
    this._booleanValue.value = "false";
    this._stringValue.value = "";
    this._intValue.value = "";

    this._prefNameInputElt.focus();

    window.addEventListener("keypress", this.handleKeypress, false);
  },

  // When we want to cancel/hide the new pref dialog / un-shield the prefs list
  hide: function AC_hide() {
    this._newPrefsDialog.classList.remove("show");
    this._prefsShield.removeAttribute("shown");

    window.removeEventListener("keypress", this.handleKeypress, false);
  },

  // Watch user key input so we can provide Enter key action, commit input values
  handleKeypress: function AC_handleKeypress(aEvent) {
    // Close our VKB on new pref enter key press
    if (aEvent.keyCode == VKB_ENTER_KEY)
      aEvent.target.blur();
  },

  // New prefs create dialog only allows creating a non-existing preference, doesn't allow for
  // Changing an existing one on-the-fly, tap existing/displayed line item pref for that
  create: function AC_create(aEvent) {
    if (this._positiveButton.getAttribute("disabled") == "true") {
      return;
    }

    switch(this.type) {
      case "boolean":
        Services.prefs.setBoolPref(this._prefNameInputElt.value, (this._booleanValue.value == "true") ? true : false);
        break;
      case "string":
        Services.prefs.setCharPref(this._prefNameInputElt.value, this._stringValue.value);
        break;
      case "int":
        Services.prefs.setIntPref(this._prefNameInputElt.value, this._intValue.value);
        break;
    }

    // Ensure pref adds flushed to disk immediately
    Services.prefs.savePrefFile(null);

    this.hide();
  },

  // Display proper positive button text/state on new prefs name input focus
  focusName: function AC_focusName(aEvent) {
    this._updatePositiveButton(aEvent.target.value);
  },

  // Display proper positive button text/state as user changes new prefs name
  updateName: function AC_updateName(aEvent) {
    this._updatePositiveButton(aEvent.target.value);
  },

  // In new prefs dialog, bool prefs are <input type="text">, as they aren't yet tied to an
  // Actual Services.prefs.*etBoolPref()
  toggleBoolValue: function AC_toggleBoolValue() {
    this._booleanValue.value = (this._booleanValue.value == "true" ? "false" : "true");
  }
}


/* ============================== AboutConfig ==============================
 *
 * Main AboutConfig object and methods
 *
 * Implements User Interfaces for maintenance of a list of Preference settings
 *
 */
var AboutConfig = {

  contextMenuLINode: null,
  filterInput: null,
  _filterPrevInput: null,
  _filterChangeTimer: null,
  _prefsContainer: null,
  _loadingContainer: null,
  _list: null,

  // Init the main AboutConfig dialog
  init: function AC_init() {
    this.filterInput = document.getElementById("filter-input");
    this._prefsContainer = document.getElementById("prefs-container");
    this._loadingContainer = document.getElementById("loading-container");

    let list = Services.prefs.getChildList("");
    this._list = list.sort().map( function AC_getMapPref(aPref) {
      return new Pref(aPref);
    }, this);

    // Display the current prefs list (retains searchFilter value)
    this.bufferFilterInput();

    // Setup the prefs observers
    Services.prefs.addObserver("", this, false);
  },

  // Uninit the main AboutConfig dialog
  uninit: function AC_uninit() {
    // Remove the prefs observer
    Services.prefs.removeObserver("", this);
  },

  // Clear the filterInput value, to display the entire list
  clearFilterInput: function AC_clearFilterInput() {
    this.filterInput.value = "";
    this.bufferFilterInput();
  },

  // Buffer down rapid changes in filterInput value from keyboard
  bufferFilterInput: function AC_bufferFilterInput() {
    if (this._filterChangeTimer) {
      clearTimeout(this._filterChangeTimer);
    }

    this._filterChangeTimer = setTimeout((function() {
      this._filterChangeTimer = null;
      // Display updated prefs list when filterInput value settles
      this._displayNewList();
    }).bind(this), FILTER_CHANGE_TRIGGER);
  },

  // Update displayed list when filterInput value changes
  _displayNewList: function AC_displayNewList() {
    // This survives the search filter value past a page refresh
    this.filterInput.setAttribute("value", this.filterInput.value);

    // Don't start new filter search if same as last
    if (this.filterInput.value == this._filterPrevInput) {
      return;
    }
    this._filterPrevInput = this.filterInput.value;

    // Clear list item selection / context menu, prefs list, get first buffer, set scrolling on
    this.selected = "";
    this._clearPrefsContainer();
    this._addMorePrefsToContainer();
    window.onscroll = this.onScroll.bind(this);

    // Pause for screen to settle, then ensure at top
    setTimeout((function() {
      window.scrollTo(0, 0);
    }).bind(this), INITIAL_PAGE_DELAY);
  },

  // Clear the displayed preferences list
  _clearPrefsContainer: function AC_clearPrefsContainer() {
    // Quick clear the prefsContainer list
    let empty = this._prefsContainer.cloneNode(false);
    this._prefsContainer.parentNode.replaceChild(empty, this._prefsContainer); 
    this._prefsContainer = empty;

    // Quick clear the prefs li.HTML list
    this._list.forEach(function(item) {
      delete item.li;
    });
  },

  // Get a small manageable block of prefs items, and add them to the displayed list
  _addMorePrefsToContainer: function AC_addMorePrefsToContainer() {
    // Create filter regex
    let filterExp = this.filterInput.value ?
      new RegExp(this.filterInput.value, "i") : null;

    // Get a new block for the display list
    let prefsBuffer = [];
    for (let i = 0; i < this._list.length && prefsBuffer.length < PREFS_BUFFER_MAX; i++) {
      if (!this._list[i].li && this._list[i].test(filterExp)) {
        prefsBuffer.push(this._list[i]);
      }
    }

    // Add the new block to the displayed list
    for (let i = 0; i < prefsBuffer.length; i++) {
      this._prefsContainer.appendChild(prefsBuffer[i].getOrCreateNewLINode());
    }

    // Determine if anything left to add later by scrolling
    let anotherPrefsBufferRemains = false;
    for (let i = 0; i < this._list.length; i++) {
      if (!this._list[i].li && this._list[i].test(filterExp)) {
        anotherPrefsBufferRemains = true;
        break;
      }
    }

    if (anotherPrefsBufferRemains) {
      // If still more could be displayed, show the throbber
      this._loadingContainer.style.display = "block";
    } else {
      // If no more could be displayed, hide the throbber, and stop noticing scroll events
      this._loadingContainer.style.display = "none";
      window.onscroll = null;
    }
  },

  // If scrolling at the bottom, maybe add some more entries
  onScroll: function AC_onScroll(aEvent) {
    if (this._prefsContainer.scrollHeight - (window.pageYOffset + window.innerHeight) < PAGE_SCROLL_TRIGGER) {
      if (!this._filterChangeTimer) {
        this._addMorePrefsToContainer();
      }
    }
  },


  // Return currently selected list item node
  get selected() {
    return document.querySelector(".pref-item.selected");
  },

  // Set list item node as selected
  set selected(aSelection) {
    let currentSelection = this.selected;
    if (aSelection == currentSelection) {
      return;
    }

    // Clear any previous selection
    if (currentSelection) {
      currentSelection.classList.remove("selected");
      currentSelection.removeEventListener("keypress", this.handleKeypress, false);
    }

    // Set any current selection
    if (aSelection) {
      aSelection.classList.add("selected");
      aSelection.addEventListener("keypress", this.handleKeypress, false);
    }
  },

  // Watch user key input so we can provide Enter key action, commit input values
  handleKeypress: function AC_handleKeypress(aEvent) {
    if (aEvent.keyCode == VKB_ENTER_KEY)
      aEvent.target.blur();
  },

  // Return the target list item node of an action event
  getLINodeForEvent: function AC_getLINodeForEvent(aEvent) {
    let node = aEvent.target;
    while (node && node.nodeName != "li") {
      node = node.parentNode;
    }

    return node;
  },

  // Return a pref of a list item node
  _getPrefForNode: function AC_getPrefForNode(aNode) {
    let pref = aNode.getAttribute("name");

    return new Pref(pref);
  },

  // When list item name or value are tapped
  selectOrToggleBoolPref: function AC_selectOrToggleBoolPref(aEvent) {
    let node = this.getLINodeForEvent(aEvent);

    // If not already selected, just do so
    if (this.selected != node) {
      this.selected = node;
      return;
    }

    // If already selected, and value is boolean, toggle it
    let pref = this._getPrefForNode(node);
    if (pref.type != Services.prefs.PREF_BOOL) {
      return;
    }

    this.toggleBoolPref(aEvent);
  },

  // When finalizing list input values due to blur
  setIntOrStringPref: function AC_setIntOrStringPref(aEvent) {
    let node = this.getLINodeForEvent(aEvent);

    // Skip if locked
    let pref = this._getPrefForNode(node);
    if (pref.locked) {
      return;
    }

    // Boolean inputs blur to remove focus from "button"
    if (pref.type == Services.prefs.PREF_BOOL) {
      return;
    }

    // String and Int inputs change / commit on blur
    pref.value = aEvent.target.value;
  },

  // When we reset a pref to it's default value (note resetting a user created pref will delete it)
  resetDefaultPref: function AC_resetDefaultPref(aEvent) {
    let node = this.getLINodeForEvent(aEvent);

    // If not already selected, do so
    if (this.selected != node) {
      this.selected = node;
    }

    // Reset will handle any locked condition
    let pref = this._getPrefForNode(node);
    pref.reset();

    // Ensure pref reset flushed to disk immediately
    Services.prefs.savePrefFile(null);
  },

  // When we want to toggle a bool pref
  toggleBoolPref: function AC_toggleBoolPref(aEvent) {
    let node = this.getLINodeForEvent(aEvent);

    // Skip if locked, or not boolean
    let pref = this._getPrefForNode(node);
    if (pref.locked) {
      return;
    }

    // Toggle, and blur to remove field focus
    pref.value = !pref.value;
    aEvent.target.blur();
  },

  // When Int inputs have their Up or Down arrows toggled
  incrOrDecrIntPref: function AC_incrOrDecrIntPref(aEvent, aInt) {
    let node = this.getLINodeForEvent(aEvent);

    // Skip if locked
    let pref = this._getPrefForNode(node);
    if (pref.locked) {
      return;
    }

    pref.value += aInt;
  },

  // Observe preference changes
  observe: function AC_observe(aSubject, aTopic, aPrefName) {
    let pref = new Pref(aPrefName);

    // Ignore uninteresting changes, and avoid "private" preferences
    if (aTopic != "nsPref:changed") {
      return;
    }

    // If pref type invalid, refresh display as user reset/removed an item from the list
    if (pref.type == Services.prefs.PREF_INVALID) {
      document.location.reload();
      return;
    }

    // If pref onscreen, update in place.
    let item = document.querySelector(".pref-item[name=\"" + CSS.escape(pref.name) + "\"]");
    if (item) {
      item.setAttribute("value", pref.value);
      let input = item.querySelector("input");
      input.setAttribute("value", pref.value);
      input.value = pref.value;

      pref.default ?
        item.querySelector(".reset").setAttribute("disabled", "true") :
        item.querySelector(".reset").removeAttribute("disabled");
      return;
    }

    // If pref not already in list, refresh display as it's being added
    let anyWhere = this._list.filter(i => { return i.name == pref.name });
    if (!anyWhere.length) {
      document.location.reload();
    }
  },

  // Quick context menu helpers for about:config
  clipboardCopy: function AC_clipboardCopy(aField) {
    let pref = this._getPrefForNode(this.contextMenuLINode);
    if (aField == 'name') {
      gClipboardHelper.copyString(pref.name);
    } else {
      gClipboardHelper.copyString(pref.value);
    }
  }
}


/* ============================== Pref ==============================
 *
 * Individual Preference object / methods
 *
 * Defines a Pref object, a document list item tied to Preferences Services
 * And the methods by which they interact.
 *
 */
function Pref(aName) {
  this.name = aName;
}

Pref.prototype = {
  get type() {
    return Services.prefs.getPrefType(this.name);
  },

  get value() {
    switch (this.type) {
      case Services.prefs.PREF_BOOL:
        return Services.prefs.getBoolPref(this.name);
      case Services.prefs.PREF_INT:
        return Services.prefs.getIntPref(this.name);
      case Services.prefs.PREF_STRING:
      default:
        return Services.prefs.getCharPref(this.name);
    }

  },
  set value(aPrefValue) {
    switch (this.type) {
      case Services.prefs.PREF_BOOL:
        Services.prefs.setBoolPref(this.name, aPrefValue);
        break;
      case Services.prefs.PREF_INT:
        Services.prefs.setIntPref(this.name, aPrefValue);
        break;
      case Services.prefs.PREF_STRING:
      default:
        Services.prefs.setCharPref(this.name, aPrefValue);
    }

    // Ensure pref change flushed to disk immediately
    Services.prefs.savePrefFile(null);
  },

  get default() {
    return !Services.prefs.prefHasUserValue(this.name);
  },

  get locked() {
    return Services.prefs.prefIsLocked(this.name);
  },

  reset: function AC_reset() {
    Services.prefs.clearUserPref(this.name);
  },

  test: function AC_test(aValue) {
    return aValue ? aValue.test(this.name) : true;
  },

  // Get existing or create new LI node for the pref
  getOrCreateNewLINode: function AC_getOrCreateNewLINode() {
    if (!this.li) {
      this.li = document.createElement("li");

      this.li.className = "pref-item";
      this.li.setAttribute("name", this.name);

      // Click callback to ensure list item selected even on no-action tap events
      this.li.addEventListener("click",
        function(aEvent) {
          AboutConfig.selected = AboutConfig.getLINodeForEvent(aEvent);
        },
        false
      );

      // Contextmenu callback to identify selected list item
      this.li.addEventListener("contextmenu",
        function(aEvent) {
          AboutConfig.contextMenuLINode = AboutConfig.getLINodeForEvent(aEvent);
        },
        false
      );

      this.li.setAttribute("contextmenu", "prefs-context-menu");

      // Create list item outline, bind to object actions
      this.li.innerHTML =
        "<div class='pref-name' " +
            "onclick='AboutConfig.selectOrToggleBoolPref(event);'>" +
            this.name +
        "</div>" +
        "<div class='pref-item-line'>" +
          "<input class='pref-value' value='' " +
            "onblur='AboutConfig.setIntOrStringPref(event);' " +
            "onclick='AboutConfig.selectOrToggleBoolPref(event);'>" +
          "</input>" +
          "<div class='pref-button reset' " +
            "onclick='AboutConfig.resetDefaultPref(event);'>" +
            gStringBundle.GetStringFromName("pref.resetButton") +
          "</div>" +
          "<div class='pref-button toggle' " +
            "onclick='AboutConfig.toggleBoolPref(event);'>" +
            gStringBundle.GetStringFromName("pref.toggleButton") +
          "</div>" +
          "<div class='pref-button up' " +
            "onclick='AboutConfig.incrOrDecrIntPref(event, 1);'>" +
          "</div>" +
          "<div class='pref-button down' " +
            "onclick='AboutConfig.incrOrDecrIntPref(event, -1);'>" +
          "</div>" +
        "</div>";

      // Delay providing the list item values, until the LI is returned and added to the document
      setTimeout(this._valueSetup.bind(this), INNERHTML_VALUE_DELAY);
    }

    return this.li;
  },

  // Initialize list item object values
  _valueSetup: function AC_valueSetup() {

    this.li.setAttribute("type", this.type);
    this.li.setAttribute("value", this.value);

    let valDiv = this.li.querySelector(".pref-value");
    valDiv.value = this.value;

    switch(this.type) {
      case Services.prefs.PREF_BOOL:
        valDiv.setAttribute("type", "button");
        this.li.querySelector(".up").setAttribute("disabled", true);
        this.li.querySelector(".down").setAttribute("disabled", true);
        break;
      case Services.prefs.PREF_STRING:
        valDiv.setAttribute("type", "text");
        this.li.querySelector(".up").setAttribute("disabled", true);
        this.li.querySelector(".down").setAttribute("disabled", true);
        this.li.querySelector(".toggle").setAttribute("disabled", true);
        break;
      case Services.prefs.PREF_INT:
        valDiv.setAttribute("type", "number");
        this.li.querySelector(".toggle").setAttribute("disabled", true);
        break;
    }

    this.li.setAttribute("default", this.default);
    if (this.default) {
      this.li.querySelector(".reset").setAttribute("disabled", true);
    }

    if (this.locked) {
      valDiv.setAttribute("disabled", this.locked);
      this.li.querySelector(".pref-name").setAttribute("locked", true);
    }
  }
}

