/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Search object that returns results at different times.
 * First, the search that returns results asynchronously.
 */
function AutoCompleteAsyncSearch(aName, aResult) {
  this.name = aName;
  this._result = aResult;
}
AutoCompleteAsyncSearch.prototype = Object.create(AutoCompleteSearchBase.prototype);
AutoCompleteAsyncSearch.prototype.startSearch = function(aSearchString, 
                                                         aSearchParam, 
                                                         aPreviousResult, 
                                                         aListener) {
  setTimeout(this._returnResults.bind(this), 500, aListener);
};

AutoCompleteAsyncSearch.prototype._returnResults = function(aListener) {
  var result = this._result;

  result.searchResult = Ci.nsIAutoCompleteResult.RESULT_SUCCESS;
  aListener.onSearchResult(this, result);
};

/**
 * The synchronous version
 */
function AutoCompleteSyncSearch(aName, aResult) {
  this.name = aName;
  this._result = aResult;
}
AutoCompleteSyncSearch.prototype = Object.create(AutoCompleteAsyncSearch.prototype);
AutoCompleteSyncSearch.prototype.startSearch = function(aSearchString, 
                                                        aSearchParam, 
                                                        aPreviousResult, 
                                                        aListener) {
  this._returnResults(aListener);
};

/**
 * Results object
 */
function AutoCompleteResult(aValues, aDefaultIndex) {
  this._values = aValues;
  this.defaultIndex = aDefaultIndex;
}
AutoCompleteResult.prototype = Object.create(AutoCompleteResultBase.prototype);


/** 
 * Test AutoComplete with multiple AutoCompleteSearch sources, with one of them
 * (index != 0) returning before the rest.
 */
function run_test() {
  do_test_pending();

  var results = ["mozillaTest"];
  var inputStr = "moz";

  // Async search
  var asyncSearch = new AutoCompleteAsyncSearch("Async", 
                                                new AutoCompleteResult(results, -1));
  // Sync search
  var syncSearch = new AutoCompleteSyncSearch("Sync",
                                              new AutoCompleteResult(results, 0));
  
  // Register searches so AutoCompleteController can find them
  registerAutoCompleteSearch(asyncSearch);
  registerAutoCompleteSearch(syncSearch);
    
  var controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);  
  
  // Make an AutoCompleteInput that uses our searches
  // and confirms results on search complete.
  // Async search MUST be FIRST to trigger the bug this tests.
  var input = new AutoCompleteInputBase([asyncSearch.name, syncSearch.name]);
  input.completeDefaultIndex = true;
  input.textValue = inputStr;

  // Caret must be at the end. Autofill doesn't happen unless you're typing
  // characters at the end.
  var strLen = inputStr.length;
  input.selectTextRange(strLen, strLen);

  controller.input = input;
  controller.startSearch(inputStr);

  input.onSearchComplete = function() {
    do_check_eq(input.textValue, results[0]);

    // Unregister searches
    unregisterAutoCompleteSearch(asyncSearch);
    unregisterAutoCompleteSearch(syncSearch);
    do_test_finished();
  };
}
