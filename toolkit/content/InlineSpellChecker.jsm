/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Inline spellcheck code
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var EXPORTED_SYMBOLS = [ "InlineSpellChecker" ];
var gLanguageBundle;
var gRegionBundle;

function InlineSpellChecker(aEditor) {
  this.init(aEditor);
}

InlineSpellChecker.prototype = {
  // Call this function to initialize for a given editor
  init: function(aEditor)
  {
    this.uninit();
    this.mEditor = aEditor;
    try {
      this.mInlineSpellChecker = this.mEditor.getInlineSpellChecker(true);
      // note: this might have been NULL if there is no chance we can spellcheck
    } catch(e) {
      this.mInlineSpellChecker = null;
    }
  },

  // call this to clear state
  uninit: function()
  {
    this.mInlineSpellChecker = null;
    this.mOverMisspelling = false;
    this.mMisspelling = "";
    this.mMenu = null;
    this.mSpellSuggestions = [];
    this.mSuggestionItems = [];
    this.mDictionaryMenu = null;
    this.mDictionaryNames = [];
    this.mDictionaryItems = [];
  },

  // for each UI event, you must call this function, it will compute the
  // word the cursor is over
  initFromEvent: function(rangeParent, rangeOffset)
  {
    this.mOverMisspelling = false;

    if (!rangeParent || !this.mInlineSpellChecker)
      return;

    var selcon = this.mEditor.selectionController;
    var spellsel = selcon.getSelection(selcon.SELECTION_SPELLCHECK);
    if (spellsel.rangeCount == 0)
      return; // easy case - no misspellings

    var range = this.mInlineSpellChecker.getMisspelledWord(rangeParent,
                                                          rangeOffset);
    if (! range)
      return; // not over a misspelled word

    this.mMisspelling = range.toString();
    this.mOverMisspelling = true;
    this.mWordNode = rangeParent;
    this.mWordOffset = rangeOffset;
  },

  // returns false if there should be no spellchecking UI enabled at all, true
  // means that you can at least give the user the ability to turn it on.
  get canSpellCheck()
  {
    // inline spell checker objects will be created only if there are actual
    // dictionaries available
    return (this.mInlineSpellChecker != null);
  },

  // Whether spellchecking is enabled in the current box
  get enabled()
  {
    return (this.mInlineSpellChecker &&
            this.mInlineSpellChecker.enableRealTimeSpell);
  },
  set enabled(isEnabled)
  {
    if (this.mInlineSpellChecker)
      this.mEditor.setSpellcheckUserOverride(isEnabled);
  },

  // returns true if the given event is over a misspelled word
  get overMisspelling()
  {
    return this.mOverMisspelling;
  },

  // this prepends up to "maxNumber" suggestions at the given menu position
  // for the word under the cursor. Returns the number of suggestions inserted.
  addSuggestionsToMenu: function(menu, insertBefore, maxNumber)
  {
    if (! this.mInlineSpellChecker || ! this.mOverMisspelling)
      return 0; // nothing to do

    var spellchecker = this.mInlineSpellChecker.spellChecker;
    try {
      if (! spellchecker.CheckCurrentWord(this.mMisspelling))
        return 0;  // word seems not misspelled after all (?)
    } catch(e) {
        return 0;
    }

    this.mMenu = menu;
    this.mSpellSuggestions = [];
    this.mSuggestionItems = [];
    for (var i = 0; i < maxNumber; i ++) {
      var suggestion = spellchecker.GetSuggestedWord();
      if (! suggestion.length)
        break;
      this.mSpellSuggestions.push(suggestion);

      var item = menu.ownerDocument.createElement("menuitem");
      this.mSuggestionItems.push(item);
      item.setAttribute("label", suggestion);
      item.setAttribute("value", suggestion);
      // this function thing is necessary to generate a callback with the
      // correct binding of "val" (the index in this loop).
      var callback = function(me, val) { return function(evt) { me.replaceMisspelling(val); } };
      item.addEventListener("command", callback(this, i), true);
      item.setAttribute("class", "spell-suggestion");
      menu.insertBefore(item, insertBefore);
    }
    return this.mSpellSuggestions.length;
  },

  // undoes the work of addSuggestionsToMenu for the same menu
  // (call from popup hiding)
  clearSuggestionsFromMenu: function()
  {
    for (var i = 0; i < this.mSuggestionItems.length; i ++) {
      this.mMenu.removeChild(this.mSuggestionItems[i]);
    }
    this.mSuggestionItems = [];
  },

  // returns the number of dictionary languages. If insertBefore is NULL, this
  // does an append to the given menu
  addDictionaryListToMenu: function(menu, insertBefore)
  {
    this.mDictionaryMenu = menu;
    this.mDictionaryNames = [];
    this.mDictionaryItems = [];

    if (! gLanguageBundle) {
      // create the bundles for language and region
      var bundleService = Components.classes["@mozilla.org/intl/stringbundle;1"]
                                    .getService(Components.interfaces.nsIStringBundleService);
      gLanguageBundle = bundleService.createBundle(
          "chrome://global/locale/languageNames.properties");
      gRegionBundle = bundleService.createBundle(
          "chrome://global/locale/regionNames.properties");
    }

    if (! this.mInlineSpellChecker || ! this.enabled)
      return 0;
    var spellchecker = this.mInlineSpellChecker.spellChecker;
    var o1 = {}, o2 = {};
    spellchecker.GetDictionaryList(o1, o2);
    var list = o1.value;
    var listcount = o2.value;
    var curlang = "";
    try {
        curlang = spellchecker.GetCurrentDictionary();
    } catch(e) {}
    var isoStrArray;

    for (var i = 0; i < list.length; i ++) {
      // get the display name for this dictionary
      isoStrArray = list[i].split(/[-_]/);
      var displayName = "";
      if (gLanguageBundle && isoStrArray[0]) {
        try {
          displayName = gLanguageBundle.GetStringFromName(isoStrArray[0].toLowerCase());
        } catch(e) {} // ignore language bundle errors
        if (gRegionBundle && isoStrArray[1]) {
          try {
            displayName += " / " + gRegionBundle.GetStringFromName(isoStrArray[1].toLowerCase());
          } catch(e) {} // ignore region bundle errors
          if (isoStrArray[2])
            displayName += " (" + isoStrArray[2] + ")";
        }
      }

      // if we didn't get a name, just use the raw dictionary name
      if (displayName.length == 0)
        displayName = list[i];

      this.mDictionaryNames.push(list[i]);
      var item = menu.ownerDocument.createElement("menuitem");
      item.setAttribute("id", "spell-check-dictionary-" + list[i]);
      item.setAttribute("label", displayName);
      item.setAttribute("type", "radio");
      this.mDictionaryItems.push(item);
      if (curlang == list[i]) {
        item.setAttribute("checked", "true");
      } else {
        var callback = function(me, val) { return function(evt) { me.selectDictionary(val); } };
        item.addEventListener("command", callback(this, i), true);
      }
      if (insertBefore)
        menu.insertBefore(item, insertBefore);
      else
        menu.appendChild(item);
    }
    return list.length;
  },

  // undoes the work of addDictionaryListToMenu for the menu
  // (call on popup hiding)
  clearDictionaryListFromMenu: function()
  {
    for (var i = 0; i < this.mDictionaryItems.length; i ++) {
      this.mDictionaryMenu.removeChild(this.mDictionaryItems[i]);
    }
    this.mDictionaryItems = [];
  },

  // callback for selecting a dictionary
  selectDictionary: function(index)
  {
    if (! this.mInlineSpellChecker || index < 0 || index >= this.mDictionaryNames.length)
      return;
    var spellchecker = this.mInlineSpellChecker.spellChecker;
    spellchecker.SetCurrentDictionary(this.mDictionaryNames[index]);
    this.mInlineSpellChecker.spellCheckRange(null); // causes recheck
  },

  // callback for selecting a suggesteed replacement
  replaceMisspelling: function(index)
  {
    if (! this.mInlineSpellChecker || ! this.mOverMisspelling)
      return;
    if (index < 0 || index >= this.mSpellSuggestions.length)
      return;
    this.mInlineSpellChecker.replaceWord(this.mWordNode, this.mWordOffset,
                                         this.mSpellSuggestions[index]);
  },

  // callback for enabling or disabling spellchecking
  toggleEnabled: function()
  {
    this.mEditor.setSpellcheckUserOverride(!this.mInlineSpellChecker.enableRealTimeSpell);
  },

  // callback for adding the current misspelling to the user-defined dictionary
  addToDictionary: function()
  {
    this.mInlineSpellChecker.addWordToDictionary(this.mMisspelling);
  },
  ignoreWord: function()
  {
    this.mInlineSpellChecker.ignoreWord(this.mMisspelling);
  }
};
