/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;

/**
 * Unit test for Bug 566489 - Inline Autocomplete.
 */


/**
 * Dummy nsIAutoCompleteInput source that returns
 * the given list of AutoCompleteSearch names. 
 */
function AutoCompleteInput(aSearches) {
  this.searches = aSearches;
}
AutoCompleteInput.prototype = {
  constructor: AutoCompleteInput, 
  
  // Array of AutoCompleteSearch names
  searches: null,
  
  minResultsForPopup: 0,
  timeout: 10,
  searchParam: "",
  textValue: "",
  disableAutoComplete: false,  
  completeDefaultIndex: true,

  // Text selection range
  selStart: 0,
  selEnd: 0,
  get selectionStart() {
    return selStart;
  },
  get selectionEnd() {
    return selEnd;
  },
  selectTextRange: function(aStart, aEnd) {
    selStart = aStart;
    selEnd = aEnd;
  },
  
  get searchCount() {
    return this.searches.length;
  },
  
  getSearchAt: function(aIndex) {
    return this.searches[aIndex];
  },
  
  onSearchBegin: function() {},
  onSearchComplete: function() {},
  
  popupOpen: false,  
  
  popup: { 
    setSelectedIndex: function(aIndex) {},
    invalidate: function() {},

    // nsISupports implementation
    QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                           Ci.nsIAutoCompletePopup])
  },
    
  // nsISupports implementation
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIAutoCompleteInput])
}



/** 
 * nsIAutoCompleteResult implementation
 */
function AutoCompleteResult(aValues, aComments, aStyles) {
  this._values = aValues;
  this._comments = aComments;
  this._styles = aStyles;
}
AutoCompleteResult.prototype = {
  constructor: AutoCompleteResult,
  
  // Arrays
  _values: null,
  _comments: null,
  _styles: null,
  
  searchString: "",
  searchResult: Ci.nsIAutoCompleteResult.RESULT_SUCCESS,
  
  defaultIndex: 0,
  isURLResult: true,

  get matchCount() {
    return this._values.length;
  },

  getValueAt: function(aIndex) {
    return this._values[aIndex];
  },

  getLabelAt: function(aIndex) {
    return this.getValueAt(aIndex);
  },
  
  getCommentAt: function(aIndex) {
    return this._comments[aIndex];
  },
  
  getStyleAt: function(aIndex) {
    return this._styles[aIndex];
  },
  
  getImageAt: function(aIndex) {
    return "";
  },

  removeValueAt: function (aRowIndex, aRemoveFromDb) {},

  // nsISupports implementation
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIAutoCompleteResult])
}


/** 
 * nsIAutoCompleteSearch implementation that always returns
 * the same result set.
 */
function AutoCompleteSearch(aName, aResult) {
  this.name = aName;
  this._result = aResult;
}
AutoCompleteSearch.prototype = {
  constructor: AutoCompleteSearch,
  
  // Search name. Used by AutoCompleteController
  name: null,

  // AutoCompleteResult
  _result: null,

  
  /**
   * Return the same result set for every search
   */
  startSearch: function(aSearchString, 
                        aSearchParam, 
                        aPreviousResult, 
                        aListener) 
  {
    aListener.onSearchResult(this, this._result);
  },
  
  stopSearch: function() {},

  // nsISupports implementation
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIFactory,
                                         Ci.nsIAutoCompleteSearch]),
  
  // nsIFactory implementation
  createInstance: function(outer, iid) {
    return this.QueryInterface(iid);
  }
}


/** 
 * Helper to register an AutoCompleteSearch with the given name.
 * Allows the AutoCompleteController to find the search.
 */
function registerAutoCompleteSearch(aSearch) {
  var name = "@mozilla.org/autocomplete/search;1?name=" + aSearch.name;

  var uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].
                      getService(Ci.nsIUUIDGenerator);
  var cid = uuidGenerator.generateUUID();

  var desc = "Test AutoCompleteSearch";
  
  var componentManager = Components.manager
                                   .QueryInterface(Ci.nsIComponentRegistrar);
  componentManager.registerFactory(cid, desc, name, aSearch);

  // Keep the id on the object so we can unregister later
  aSearch.cid = cid; 
}


/** 
 * Helper to unregister an AutoCompleteSearch. 
 */
function unregisterAutoCompleteSearch(aSearch) {
  var componentManager = Components.manager
                                   .QueryInterface(Ci.nsIComponentRegistrar);  
  componentManager.unregisterFactory(aSearch.cid, aSearch);
}


/**
 * The array of autocomplete test data to run.
 */
var tests = [
  {
    searchValues: ["mozilla.org"],        // Autocomplete results
    inputString: "moz",                   // The search string
    expectedAutocomplete: "mozilla.org",  // The string we expect to be autocompleted to
    expectedSelStart: 3,                  // The range of the selection we expect
    expectedSelEnd: 11
  },
  {
    // Test URL schemes
    searchValues: ["http://www.mozilla.org", "mozNotFirstMatch.org"],
    inputString: "moz",
    expectedAutocomplete: "mozilla.org",
    expectedSelStart: 3,
    expectedSelEnd: 11
  },
  {
    // Test URL schemes
    searchValues: ["ftp://ftp.mozilla.org/"],
    inputString: "ft",
    expectedAutocomplete: "ftp.mozilla.org/",
    expectedSelStart: 2,
    expectedSelEnd: 16
  },
  {
    // Test the moz-action scheme, used internally for things like switch-to-tab
    searchValues: ["moz-action:someaction,http://www.mozilla.org", "mozNotFirstMatch.org"],
    inputString: "moz",
    expectedAutocomplete: "mozilla.org",
    expectedSelStart: 3,
    expectedSelEnd: 11
  },
  {
    // Test that we autocomplete to the first match, not necessarily the first entry
    searchValues: ["unimportantTLD.org/moz", "mozilla.org"],
    inputString: "moz",
    expectedAutocomplete: "mozilla.org",
    expectedSelStart: 3,
    expectedSelEnd: 11
  },
  {
    // Test that we only autocomplete to the next URL separator (/)
    searchValues: ["http://mozilla.org/credits/morecredits"],
    inputString: "moz",
    expectedAutocomplete: "mozilla.org/",
    expectedSelStart: 3,
    expectedSelEnd: 12
  },
  {
    // Test that we only autocomplete to the next URL separator (/)
    searchValues: ["http://mozilla.org/credits/morecredits"],
    inputString: "mozilla.org/cr",
    expectedAutocomplete: "mozilla.org/credits/",
    expectedSelStart: 14,
    expectedSelEnd: 20
  },
  {
    // Test that we only autocomplete to before the next URL separator (#)
    searchValues: ["http://mozilla.org/credits#VENTNOR"],
    inputString: "mozilla.org/cr",
    expectedAutocomplete: "mozilla.org/credits",
    expectedSelStart: 14,
    expectedSelEnd: 19
  },
  {
    // Test that we only autocomplete to before the next URL separator (?)
    searchValues: ["http://mozilla.org/credits?mozilla=awesome"],
    inputString: "mozilla.org/cr",
    expectedAutocomplete: "mozilla.org/credits",
    expectedSelStart: 14,
    expectedSelEnd: 19
  },
  {
    // Test that schemes are removed from the input
    searchValues: ["http://www.mozilla.org/credits"],
    inputString: "http://mozi",
    expectedAutocomplete: "http://mozilla.org/",
    expectedSelStart: 11,
    expectedSelEnd: 19
  },
];


/**
 * Run an individual autocomplete search, one at a time.
 */
function run_search() {
  if (tests.length == 0) {
    do_test_finished();
    return;
  }

  var test = tests.shift();

  var search = new AutoCompleteSearch("test-autofill1",
    new AutoCompleteResult(test.searchValues, ["", ""], ["", ""]));

  // Register search so AutoCompleteController can find them
  registerAutoCompleteSearch(search);

  var controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);

  // Make an AutoCompleteInput that uses our search
  // and confirms results on search complete
  var input = new AutoCompleteInput([search.name]);
  input.textValue = test.inputString;

  // Caret must be at the end. Autofill doesn't happen unless you're typing
  // characters at the end.
  var strLen = test.inputString.length;
  input.selectTextRange(strLen, strLen);

  input.onSearchComplete = function() {
    do_check_eq(input.textValue, test.expectedAutocomplete);
    do_check_eq(input.selectionStart, test.expectedSelStart);
    do_check_eq(input.selectionEnd, test.expectedSelEnd);

    // Unregister searches
    unregisterAutoCompleteSearch(search);
    run_search();
  };

  controller.input = input;
  controller.startSearch(test.inputString);
}


/** 
 */
function run_test() {
  // Search is asynchronous, so don't let the test finish immediately
  do_test_pending();
  run_search();
}

