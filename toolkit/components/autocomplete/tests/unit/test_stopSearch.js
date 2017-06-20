/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Purpose of the test is to check that a stopSearch call comes always before a
 * startSearch call.
 */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");


/**
 * Dummy nsIAutoCompleteInput source that returns
 * the given list of AutoCompleteSearch names.
 *
 * Implements only the methods needed for this test.
 */
function AutoCompleteInput(aSearches) {
  this.searches = aSearches;
}
AutoCompleteInput.prototype = {
  constructor: AutoCompleteInput,
  minResultsForPopup: 0,
  timeout: 10,
  searchParam: "",
  textValue: "hello",
  disableAutoComplete: false,
  completeDefaultIndex: false,
  set popupOpen(val) { return val; }, // ignore
  get popupOpen() { return false; },
  get searchCount() { return this.searches.length; },
  getSearchAt(aIndex) { return this.searches[aIndex]; },
  onSearchBegin() {},
  onSearchComplete() {},
  onTextReverted() {},
  onTextEntered() {},
  popup: {
    selectBy() {},
    invalidate() {},
    set selectedIndex(val) { return val; }, // ignore
    get selectedIndex() { return -1 },
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompletePopup])
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompleteInput])
}


/**
 * nsIAutoCompleteSearch implementation.
 */
function AutoCompleteSearch(aName) {
  this.name = aName;
}
AutoCompleteSearch.prototype = {
  constructor: AutoCompleteSearch,
  stopSearchInvoked: true,
  startSearch(aSearchString, aSearchParam, aPreviousResult, aListener) {
    print("Check stop search has been called");
    do_check_true(this.stopSearchInvoked);
    this.stopSearchInvoked = false;
  },
  stopSearch() {
    this.stopSearchInvoked = true;
  },
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIFactory,
    Ci.nsIAutoCompleteSearch
  ]),
  createInstance(outer, iid) {
    return this.QueryInterface(iid);
  }
}


/**
 * Helper to register an AutoCompleteSearch with the given name.
 * Allows the AutoCompleteController to find the search.
 */
function registerAutoCompleteSearch(aSearch) {
  let name = "@mozilla.org/autocomplete/search;1?name=" + aSearch.name;
  let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].
                      getService(Ci.nsIUUIDGenerator);
  let cid = uuidGenerator.generateUUID();
  let desc = "Test AutoCompleteSearch";
  let componentManager = Components.manager
                                   .QueryInterface(Ci.nsIComponentRegistrar);
  componentManager.registerFactory(cid, desc, name, aSearch);
  // Keep the id on the object so we can unregister later
  aSearch.cid = cid;
}


/**
 * Helper to unregister an AutoCompleteSearch.
 */
function unregisterAutoCompleteSearch(aSearch) {
  let componentManager = Components.manager
                                   .QueryInterface(Ci.nsIComponentRegistrar);
  componentManager.unregisterFactory(aSearch.cid, aSearch);
}


var gTests = [
  function(controller) {
    print("handleText");
    controller.input.textValue = "hel";
    controller.handleText();
  },
  function(controller) {
    print("handleStartComposition");
    controller.handleStartComposition();
  },
  function(controller) {
    print("handleEndComposition");
    controller.handleEndComposition();
    // an input event always follows compositionend event.
    controller.handleText();
  },
  function(controller) {
    print("handleEscape");
    controller.handleEscape();
  },
  function(controller) {
    print("handleEnter");
    controller.handleEnter(false);
  },
  function(controller) {
    print("handleTab");
    controller.handleTab();
  },

  function(controller) {
    print("handleKeyNavigation");
    controller.handleKeyNavigation(Ci.nsIDOMKeyEvent.DOM_VK_UP);
  },
];


var gSearch;
var gCurrentTest;
function run_test() {
  // Make an AutoCompleteSearch that always returns nothing
  gSearch = new AutoCompleteSearch("test");
  registerAutoCompleteSearch(gSearch);

  let controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);

  // Make an AutoCompleteInput that uses our search.
  let input = new AutoCompleteInput([gSearch.name]);
  controller.input = input;

  input.onSearchBegin = function() {
    do_execute_soon(function() {
      gCurrentTest(controller);
    });
  };
  input.onSearchComplete = function() {
    run_next_test(controller);
  }

  // Search is asynchronous, so don't let the test finish immediately
  do_test_pending();

  run_next_test(controller);
}

function run_next_test(controller) {
  if (gTests.length == 0) {
    unregisterAutoCompleteSearch(gSearch);
    controller.stopSearch();
    controller.input = null;
    do_test_finished();
    return;
  }

  gCurrentTest = gTests.shift();
  controller.startSearch("hello");
}
