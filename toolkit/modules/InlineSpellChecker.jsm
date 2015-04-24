/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [ "InlineSpellChecker",
                          "SpellCheckHelper" ];
var gLanguageBundle;
var gRegionBundle;
const MAX_UNDO_STACK_DEPTH = 1;

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

this.InlineSpellChecker = function InlineSpellChecker(aEditor) {
  this.init(aEditor);
  this.mAddedWordStack = []; // We init this here to preserve it between init/uninit calls
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

  initFromRemote: function(aSpellInfo)
  {
    if (this.mRemote)
      throw new Error("Unexpected state");
    this.uninit();

    if (!aSpellInfo)
      return;
    this.mInlineSpellChecker = this.mRemote = new RemoteSpellChecker(aSpellInfo);
    this.mOverMisspelling = aSpellInfo.overMisspelling;
    this.mMisspelling = aSpellInfo.misspelling;
  },

  // call this to clear state
  uninit: function()
  {
    if (this.mRemote) {
      this.mRemote.uninit();
      this.mRemote = null;
    }

    this.mEditor = null;
    this.mInlineSpellChecker = null;
    this.mOverMisspelling = false;
    this.mMisspelling = "";
    this.mMenu = null;
    this.mSpellSuggestions = [];
    this.mSuggestionItems = [];
    this.mDictionaryMenu = null;
    this.mDictionaryNames = [];
    this.mDictionaryItems = [];
    this.mWordNode = null;
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
    if (this.mRemote)
      return this.mRemote.canSpellCheck;
    return this.mInlineSpellChecker != null;
  },

  get initialSpellCheckPending() {
    if (this.mRemote) {
      return this.mRemote.spellCheckPending;
    }
    return !!(this.mInlineSpellChecker &&
              !this.mInlineSpellChecker.spellChecker &&
              this.mInlineSpellChecker.spellCheckPending);
  },

  // Whether spellchecking is enabled in the current box
  get enabled()
  {
    if (this.mRemote)
      return this.mRemote.enableRealTimeSpell;
    return (this.mInlineSpellChecker &&
            this.mInlineSpellChecker.enableRealTimeSpell);
  },
  set enabled(isEnabled)
  {
    if (this.mRemote)
      this.mRemote.setSpellcheckUserOverride(isEnabled);
    else if (this.mInlineSpellChecker)
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
    if (!this.mRemote && (!this.mInlineSpellChecker || !this.mOverMisspelling))
      return 0; // nothing to do

    var spellchecker = this.mRemote || this.mInlineSpellChecker.spellChecker;
    try {
      if (!this.mRemote && !spellchecker.CheckCurrentWord(this.mMisspelling))
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

  sortDictionaryList: function(list) {
    var sortedList = [];
    for (var i = 0; i < list.length; i ++) {
      sortedList.push({"id": list[i],
                       "label": this.getDictionaryDisplayName(list[i])});
    }
    sortedList.sort(function(a, b) {
      if (a.label < b.label)
        return -1;
      if (a.label > b.label)
        return 1;
      return 0;
    });

    return sortedList;
  },

  // returns the number of dictionary languages. If insertBefore is NULL, this
  // does an append to the given menu
  addDictionaryListToMenu: function(menu, insertBefore)
  {
    this.mDictionaryMenu = menu;
    this.mDictionaryNames = [];
    this.mDictionaryItems = [];

    if (!this.enabled)
      return 0;

    var list;
    var curlang = "";
    if (this.mRemote) {
      list = this.mRemote.dictionaryList;
      curlang = this.mRemote.currentDictionary;
    }
    else if (this.mInlineSpellChecker) {
      var spellchecker = this.mInlineSpellChecker.spellChecker;
      var o1 = {}, o2 = {};
      spellchecker.GetDictionaryList(o1, o2);
      list = o1.value;
      var listcount = o2.value;
      try {
        curlang = spellchecker.GetCurrentDictionary();
      } catch(e) {}
    }

    var sortedList = this.sortDictionaryList(list);

    for (var i = 0; i < sortedList.length; i ++) {
      this.mDictionaryNames.push(sortedList[i].id);
      var item = menu.ownerDocument.createElement("menuitem");
      item.setAttribute("id", "spell-check-dictionary-" + sortedList[i].id);
      item.setAttribute("label", sortedList[i].label);
      item.setAttribute("type", "radio");
      this.mDictionaryItems.push(item);
      if (curlang == sortedList[i].id) {
        item.setAttribute("checked", "true");
      } else {
        var callback = function(me, val) {
          return function(evt) {
            me.selectDictionary(val);
          }
        };
        item.addEventListener("command", callback(this, i), true);
      }
      if (insertBefore)
        menu.insertBefore(item, insertBefore);
      else
        menu.appendChild(item);
    }
    return list.length;
  },

  // Formats a valid BCP 47 language tag based on available localized names.
  getDictionaryDisplayName: function(dictionaryName) {
    try {
      // Get the display name for this dictionary.
      let languageTagMatch = /^([a-z]{2,3}|[a-z]{4}|[a-z]{5,8})(?:[-_]([a-z]{4}))?(?:[-_]([A-Z]{2}|[0-9]{3}))?((?:[-_](?:[a-z0-9]{5,8}|[0-9][a-z0-9]{3}))*)(?:[-_][a-wy-z0-9](?:[-_][a-z0-9]{2,8})+)*(?:[-_]x(?:[-_][a-z0-9]{1,8})+)?$/i;
      var [languageTag, languageSubtag, scriptSubtag, regionSubtag, variantSubtags] = dictionaryName.match(languageTagMatch);
    } catch(e) {
      // If we weren't given a valid language tag, just use the raw dictionary name.
      return dictionaryName;
    }

    if (!gLanguageBundle) {
      // Create the bundles for language and region names.
      var bundleService = Components.classes["@mozilla.org/intl/stringbundle;1"]
                                    .getService(Components.interfaces.nsIStringBundleService);
      gLanguageBundle = bundleService.createBundle(
          "chrome://global/locale/languageNames.properties");
      gRegionBundle = bundleService.createBundle(
          "chrome://global/locale/regionNames.properties");
    }

    var displayName = "";

    // Language subtag will normally be 2 or 3 letters, but could be up to 8.
    try {
      displayName += gLanguageBundle.GetStringFromName(languageSubtag.toLowerCase());
    } catch(e) {
      displayName += languageSubtag.toLowerCase(); // Fall back to raw language subtag.
    }

    // Region subtag will be 2 letters or 3 digits.
    if (regionSubtag) {
      displayName += " (";

      try {
        displayName += gRegionBundle.GetStringFromName(regionSubtag.toLowerCase());
      } catch(e) {
        displayName += regionSubtag.toUpperCase(); // Fall back to raw region subtag.
      }

      displayName += ")";
    }

    // Script subtag will be 4 letters.
    if (scriptSubtag) {
      displayName += " / ";

      // XXX: See bug 666662 and bug 666731 for full implementation.
      displayName += scriptSubtag; // Fall back to raw script subtag.
    }

    // Each variant subtag will be 4 to 8 chars.
    if (variantSubtags)
      // XXX: See bug 666662 and bug 666731 for full implementation.
      displayName += " (" + variantSubtags.substr(1).split(/[-_]/).join(" / ") + ")"; // Collapse multiple variants.

    return displayName;
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
    if (this.mRemote) {
      this.mRemote.selectDictionary(index);
      return;
    }
    if (! this.mInlineSpellChecker || index < 0 || index >= this.mDictionaryNames.length)
      return;
    var spellchecker = this.mInlineSpellChecker.spellChecker;
    spellchecker.SetCurrentDictionary(this.mDictionaryNames[index]);
    this.mInlineSpellChecker.spellCheckRange(null); // causes recheck
  },

  // callback for selecting a suggested replacement
  replaceMisspelling: function(index)
  {
    if (this.mRemote) {
      this.mRemote.replaceMisspelling(index);
      return;
    }
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
    if (this.mRemote)
      this.mRemote.toggleEnabled();
    else
      this.mEditor.setSpellcheckUserOverride(!this.mInlineSpellChecker.enableRealTimeSpell);
  },

  // callback for adding the current misspelling to the user-defined dictionary
  addToDictionary: function()
  {
    // Prevent the undo stack from growing over the max depth
    if (this.mAddedWordStack.length == MAX_UNDO_STACK_DEPTH)
      this.mAddedWordStack.shift();

    this.mAddedWordStack.push(this.mMisspelling);
    if (this.mRemote)
      this.mRemote.addToDictionary();
    else {
      this.mInlineSpellChecker.addWordToDictionary(this.mMisspelling);
    }
  },
  // callback for removing the last added word to the dictionary LIFO fashion
  undoAddToDictionary: function()
  {
    if (this.mAddedWordStack.length > 0)
    {
      var word = this.mAddedWordStack.pop();
      if (this.mRemote)
        this.mRemote.undoAddToDictionary(word);
      else
        this.mInlineSpellChecker.removeWordFromDictionary(word);
    }
  },
  canUndo : function()
  {
    // Return true if we have words on the stack
    return (this.mAddedWordStack.length > 0);
  },
  ignoreWord: function()
  {
    if (this.mRemote)
      this.mRemote.ignoreWord();
    else
      this.mInlineSpellChecker.ignoreWord(this.mMisspelling);
  }
};

var SpellCheckHelper = {
  // Set when over a non-read-only <textarea> or editable <input>.
  EDITABLE: 0x1,

  // Set when over an <input> element of any type.
  INPUT: 0x2,

  // Set when over any <textarea>.
  TEXTAREA: 0x4,

  // Set when over any text-entry <input>.
  TEXTINPUT: 0x8,

  // Set when over an <input> that can be used as a keyword field.
  KEYWORD: 0x10,

  // Set when over an element that otherwise would not be considered
  // "editable" but is because content editable is enabled for the document.
  CONTENTEDITABLE: 0x20,

  // Set when over an <input type="number"> or other non-text field.
  NUMERIC: 0x40,

  isTargetAKeywordField(aNode, window) {
    if (!(aNode instanceof window.HTMLInputElement))
      return false;

    var form = aNode.form;
    if (!form || aNode.type == "password")
      return false;

    var method = form.method.toUpperCase();

    // These are the following types of forms we can create keywords for:
    //
    // method   encoding type       can create keyword
    // GET      *                                 YES
    //          *                                 YES
    // POST                                       YES
    // POST     application/x-www-form-urlencoded YES
    // POST     text/plain                        NO (a little tricky to do)
    // POST     multipart/form-data               NO
    // POST     everything else                   YES
    return (method == "GET" || method == "") ||
           (form.enctype != "text/plain") && (form.enctype != "multipart/form-data");
  },

  // Returns the computed style attribute for the given element.
  getComputedStyle(aElem, aProp) {
    return aElem.ownerDocument
                .defaultView
                .getComputedStyle(aElem, "").getPropertyValue(aProp);
  },

  isEditable(element, window) {
    var flags = 0;
    if (element instanceof window.HTMLInputElement) {
      flags |= this.INPUT;

      if (element.mozIsTextField(false) || element.type == "number") {
        flags |= this.TEXTINPUT;

        if (element.type == "number") {
          flags |= this.NUMERIC;
        }

        // Allow spellchecking UI on all text and search inputs.
        if (!element.readOnly &&
            (element.type == "text" || element.type == "search")) {
          flags |= this.EDITABLE;
        }
        if (this.isTargetAKeywordField(element, window))
          flags |= this.KEYWORD;
      }
    } else if (element instanceof window.HTMLTextAreaElement) {
      flags |= this.TEXTINPUT | this.TEXTAREA;
      if (!element.readOnly) {
        flags |= this.EDITABLE;
      }
    }

    if (!(flags & this.EDITABLE)) {
      var win = element.ownerDocument.defaultView;
      if (win) {
        var isEditable = false;
        try {
          var editingSession = win.QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIWebNavigation)
                                  .QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIEditingSession);
          if (editingSession.windowIsEditable(win) &&
              this.getComputedStyle(element, "-moz-user-modify") == "read-write") {
            isEditable = true;
          }
        }
        catch(ex) {
          // If someone built with composer disabled, we can't get an editing session.
        }

        if (isEditable)
          flags |= this.CONTENTEDITABLE;
      }
    }

    return flags;
  },
};

function RemoteSpellChecker(aSpellInfo) {
  this._spellInfo = aSpellInfo;
  this._suggestionGenerator = null;
}

RemoteSpellChecker.prototype = {
  get canSpellCheck() { return this._spellInfo.canSpellCheck; },
  get spellCheckPending() { return this._spellInfo.initialSpellCheckPending; },
  get overMisspelling() { return this._spellInfo.overMisspelling; },
  get enableRealTimeSpell() { return this._spellInfo.enableRealTimeSpell; },

  GetSuggestedWord() {
    if (!this._suggestionGenerator) {
      this._suggestionGenerator = (function*(spellInfo) {
        for (let i of spellInfo.spellSuggestions)
          yield i;
      })(this._spellInfo);
    }

    let next = this._suggestionGenerator.next();
    if (next.done) {
      this._suggestionGenerator = null;
      return "";
    }
    return next.value;
  },

  get currentDictionary() { return this._spellInfo.currentDictionary },
  get dictionaryList() { return this._spellInfo.dictionaryList.slice(); },

  selectDictionary(index) {
    this._spellInfo.target.sendAsyncMessage("InlineSpellChecker:selectDictionary",
                                            { index });
  },

  replaceMisspelling(index) {
    this._spellInfo.target.sendAsyncMessage("InlineSpellChecker:replaceMisspelling",
                                            { index });
  },

  toggleEnabled() { this._spellInfo.target.sendAsyncMessage("InlineSpellChecker:toggleEnabled", {}); },
  addToDictionary() {
    // This is really ugly. There is an nsISpellChecker somewhere in the
    // parent that corresponds to our current element's spell checker in the
    // child, but it's hard to access it. However, we know that
    // addToDictionary adds the word to the singleton personal dictionary, so
    // we just do that here.
    // NB: We also rely on the fact that we only ever pass an empty string in
    // as the "lang".

    let dictionary = Cc["@mozilla.org/spellchecker/personaldictionary;1"]
                       .getService(Ci.mozIPersonalDictionary);
    dictionary.addWord(this._spellInfo.misspelling, "");

    this._spellInfo.target.sendAsyncMessage("InlineSpellChecker:recheck", {});
  },
  undoAddToDictionary(word) {
    let dictionary = Cc["@mozilla.org/spellchecker/personaldictionary;1"]
                       .getService(Ci.mozIPersonalDictionary);
    dictionary.removeWord(word, "");

    this._spellInfo.target.sendAsyncMessage("InlineSpellChecker:recheck", {});
  },
  ignoreWord() {
    let dictionary = Cc["@mozilla.org/spellchecker/personaldictionary;1"]
                       .getService(Ci.mozIPersonalDictionary);
    dictionary.ignoreWord(this._spellInfo.misspelling);

    this._spellInfo.target.sendAsyncMessage("InlineSpellChecker:recheck", {});
  },
  uninit() { this._spellInfo.target.sendAsyncMessage("InlineSpellChecker:uninit", {}); }
};
