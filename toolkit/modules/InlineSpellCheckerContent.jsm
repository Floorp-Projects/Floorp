/* vim: set ts=2 sw=2 sts=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

let { SpellCheckHelper } = Cu.import("resource://gre/modules/InlineSpellChecker.jsm");

this.EXPORTED_SYMBOLS = [ "InlineSpellCheckerContent" ]

var InlineSpellCheckerContent = {
  _spellChecker: null,
  _manager: null,

  initContextMenu(event, editFlags, messageManager) {
    this._manager = messageManager;

    let spellChecker;
    if (!(editFlags & (SpellCheckHelper.TEXTAREA | SpellCheckHelper.INPUT))) {
      // Get the editor off the window.
      let win = event.target.ownerDocument.defaultView;
      let editingSession = win.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebNavigation)
                              .QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIEditingSession);
      spellChecker = this._spellChecker =
        new InlineSpellChecker(editingSession.getEditorForWindow(win));
    } else {
      // Use the element's editor.
      spellChecker = this._spellChecker =
        new InlineSpellChecker(event.target.QueryInterface(Ci.nsIDOMNSEditableElement).editor);
    }

    this._spellChecker.initFromEvent(event.rangeParent, event.rangeOffset)

    this._addMessageListeners();

    if (!spellChecker.canSpellCheck) {
      return { canSpellCheck: false,
               initialSpellCheckPending: true,
               enableRealTimeSpell: false };
    }

    if (!spellChecker.mInlineSpellChecker.enableRealTimeSpell) {
      return { canSpellCheck: true,
               initialSpellCheckPending: spellChecker.initialSpellCheckPending,
               enableRealTimeSpell: false };
    }

    let dictionaryList = {};
    let realSpellChecker = spellChecker.mInlineSpellChecker.spellChecker;
    realSpellChecker.GetDictionaryList(dictionaryList, {});

    // The original list we get is in random order. We need our list to be
    // sorted by display names.
    dictionaryList = spellChecker.sortDictionaryList(dictionaryList.value).map((obj) => {
      return obj.id;
    });
    spellChecker.mDictionaryNames = dictionaryList;

    return { canSpellCheck: spellChecker.canSpellCheck,
             initialSpellCheckPending: spellChecker.initialSpellCheckPending,
             enableRealTimeSpell: spellChecker.enabled,
             overMisspelling: spellChecker.overMisspelling,
             misspelling: spellChecker.mMisspelling,
             spellSuggestions: this._generateSpellSuggestions(),
             currentDictionary: spellChecker.mInlineSpellChecker.spellChecker.GetCurrentDictionary(),
             dictionaryList: dictionaryList };
  },

  uninitContextMenu() {
    for (let i of this._messages)
      this._manager.removeMessageListener(i, this);

    this._manager = null;
    this._spellChecker = null;
  },

  _generateSpellSuggestions() {
    let spellChecker = this._spellChecker.mInlineSpellChecker.spellChecker;
    try {
      spellChecker.CheckCurrentWord(this._spellChecker.mMisspelling);
    } catch (e) {
      return [];
    }

    let suggestions = new Array(5);
    for (let i = 0; i < 5; ++i) {
      suggestions[i] = spellChecker.GetSuggestedWord();
      if (suggestions[i].length === 0) {
        suggestions.length = i;
        break;
      }
    }

    this._spellChecker.mSpellSuggestions = suggestions;
    return suggestions;
  },

  _messages: [
      "InlineSpellChecker:selectDictionary",
      "InlineSpellChecker:replaceMisspelling",
      "InlineSpellChecker:toggleEnabled",

      "InlineSpellChecker:recheck",

      "InlineSpellChecker:uninit"
    ],

  _addMessageListeners() {
    for (let i of this._messages)
      this._manager.addMessageListener(i, this);
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "InlineSpellChecker:selectDictionary":
        this._spellChecker.selectDictionary(msg.data.index);
        break;

      case "InlineSpellChecker:replaceMisspelling":
        this._spellChecker.replaceMisspelling(msg.data.index);
        break;

      case "InlineSpellChecker:toggleEnabled":
        this._spellChecker.toggleEnabled();
        break;

      case "InlineSpellChecker:recheck":
        this._spellChecker.mInlineSpellChecker.enableRealTimeSpell = true;
        break;

      case "InlineSpellChecker:uninit":
        this.uninitContextMenu();
        break;
    }
  }
};
