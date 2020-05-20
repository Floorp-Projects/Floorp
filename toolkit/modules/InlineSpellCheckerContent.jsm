/* vim: set ts=2 sw=2 sts=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { InlineSpellChecker, SpellCheckHelper } = ChromeUtils.import(
  "resource://gre/modules/InlineSpellChecker.jsm"
);

var EXPORTED_SYMBOLS = ["InlineSpellCheckerContent"];

var InlineSpellCheckerContent = {
  _spellChecker: null,
  _actor: null,

  initContextMenu(event, editFlags, actor) {
    this._actor = actor;
    this._actor.registerDestructionObserver(this);

    let spellChecker;
    if (!(editFlags & (SpellCheckHelper.TEXTAREA | SpellCheckHelper.INPUT))) {
      // Get the editor off the window.
      let win = event.target.ownerGlobal;
      let editingSession = win.docShell.editingSession;
      spellChecker = this._spellChecker = new InlineSpellChecker(
        editingSession.getEditorForWindow(win)
      );
    } else {
      // Use the element's editor.
      spellChecker = this._spellChecker = new InlineSpellChecker(
        event.composedTarget.editor
      );
    }

    this._spellChecker.initFromEvent(event.rangeParent, event.rangeOffset);

    if (!spellChecker.canSpellCheck) {
      return {
        canSpellCheck: false,
        initialSpellCheckPending: true,
        enableRealTimeSpell: false,
      };
    }

    if (!spellChecker.mInlineSpellChecker.enableRealTimeSpell) {
      return {
        canSpellCheck: true,
        initialSpellCheckPending: spellChecker.initialSpellCheckPending,
        enableRealTimeSpell: false,
      };
    }

    if (spellChecker.initialSpellCheckPending) {
      return {
        canSpellCheck: true,
        initialSpellCheckPending: true,
        enableRealTimeSpell: true,
      };
    }

    let realSpellChecker = spellChecker.mInlineSpellChecker.spellChecker;
    let dictionaryList = realSpellChecker.GetDictionaryList();

    return {
      canSpellCheck: spellChecker.canSpellCheck,
      initialSpellCheckPending: spellChecker.initialSpellCheckPending,
      enableRealTimeSpell: spellChecker.enabled,
      overMisspelling: spellChecker.overMisspelling,
      misspelling: spellChecker.mMisspelling,
      spellSuggestions: this._generateSpellSuggestions(),
      currentDictionary: spellChecker.mInlineSpellChecker.spellChecker.GetCurrentDictionary(),
      dictionaryList,
    };
  },

  uninitContextMenu() {
    if (this._actor) {
      this._actor.unregisterDestructionObserver(this);
    }
    this._actor = null;
    this._spellChecker = null;
  },

  actorDestroyed() {
    this.uninitContextMenu();
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

  selectDictionary(localeCode) {
    this._spellChecker.selectDictionary(localeCode);
  },

  replaceMisspelling(index) {
    this._spellChecker.replaceMisspelling(index);
  },

  toggleEnabled() {
    this._spellChecker.toggleEnabled();
  },

  recheck() {
    this._spellChecker.mInlineSpellChecker.enableRealTimeSpell = true;
  },
};
