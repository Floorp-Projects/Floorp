/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Purpose of the test is to check that a stopSearch call comes always before a
 * startSearch call.
 */

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
  set popupOpen(val) {
    return val;
  }, // ignore
  get popupOpen() {
    return false;
  },
  get searchCount() {
    return this.searches.length;
  },
  getSearchAt(aIndex) {
    return this.searches[aIndex];
  },
  onSearchBegin() {},
  onSearchComplete() {},
  onTextReverted() {},
  onTextEntered() {},
  popup: {
    selectBy() {},
    invalidate() {},
    set selectedIndex(val) {
      return val;
    }, // ignore
    get selectedIndex() {
      return -1;
    },
    QueryInterface: ChromeUtils.generateQI(["nsIAutoCompletePopup"]),
  },
  QueryInterface: ChromeUtils.generateQI(["nsIAutoCompleteInput"]),
};

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
    info("Check stop search has been called");
    Assert.ok(this.stopSearchInvoked);
    this.stopSearchInvoked = false;
  },
  stopSearch() {
    this.stopSearchInvoked = true;
  },
  QueryInterface: ChromeUtils.generateQI([
    "nsIFactory",
    "nsIAutoCompleteSearch",
  ]),
  createInstance(outer, iid) {
    return this.QueryInterface(iid);
  },
};

/**
 * Helper to register an AutoCompleteSearch with the given name.
 * Allows the AutoCompleteController to find the search.
 */
function registerAutoCompleteSearch(aSearch) {
  let name = "@mozilla.org/autocomplete/search;1?name=" + aSearch.name;
  let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(
    Ci.nsIUUIDGenerator
  );
  let cid = uuidGenerator.generateUUID();
  let desc = "Test AutoCompleteSearch";
  let componentManager = Components.manager.QueryInterface(
    Ci.nsIComponentRegistrar
  );
  componentManager.registerFactory(cid, desc, name, aSearch);
  // Keep the id on the object so we can unregister later
  aSearch.cid = cid;
}

/**
 * Helper to unregister an AutoCompleteSearch.
 */
function unregisterAutoCompleteSearch(aSearch) {
  let componentManager = Components.manager.QueryInterface(
    Ci.nsIComponentRegistrar
  );
  componentManager.unregisterFactory(aSearch.cid, aSearch);
}

var gTests = [
  function(controller) {
    info("handleText");
    controller.input.textValue = "hel";
    controller.handleText();
  },
  function(controller) {
    info("handleStartComposition");
    controller.handleStartComposition();
    info("handleEndComposition");
    controller.handleEndComposition();
    // an input event always follows compositionend event.
    controller.handleText();
  },
  function(controller) {
    info("handleEscape");
    controller.handleEscape();
  },
  function(controller) {
    info("handleEnter");
    controller.handleEnter(false);
  },
  function(controller) {
    info("handleTab");
    controller.handleTab();
  },

  function(controller) {
    info("handleKeyNavigation");
    // Hardcode KeyboardEvent.DOM_VK_RIGHT, because we can't easily
    // include KeyboardEvent here.
    controller.handleKeyNavigation(0x26 /* KeyboardEvent.DOM_VK_UP */);
  },
];

add_task(async function() {
  // Make an AutoCompleteSearch that always returns nothing
  let search = new AutoCompleteSearch("test");
  registerAutoCompleteSearch(search);

  let controller = Cc["@mozilla.org/autocomplete/controller;1"].getService(
    Ci.nsIAutoCompleteController
  );

  // Make an AutoCompleteInput that uses our search.
  let input = new AutoCompleteInput([search.name]);
  controller.input = input;

  for (let testFn of gTests) {
    input.onSearchBegin = function() {
      executeSoon(() => testFn(controller));
    };
    let promise = new Promise(resolve => {
      input.onSearchComplete = function() {
        resolve();
      };
    });
    controller.startSearch("hello");
    await promise;
  }

  unregisterAutoCompleteSearch(search);
  controller.stopSearch();
  controller.input = null;
});
