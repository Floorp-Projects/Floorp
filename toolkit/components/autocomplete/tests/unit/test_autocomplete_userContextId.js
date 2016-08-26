"use strict";

Cu.import("resource://gre/modules/Promise.jsm");

function AutoCompleteInput(aSearches, aUserContextId) {
  this.searches = aSearches;
  this.userContextId = aUserContextId;
  this.popup.selectedIndex = -1;
}
AutoCompleteInput.prototype = Object.create(AutoCompleteInputBase.prototype);

function AutoCompleteSearch(aName) {
  this.name = aName;
}
AutoCompleteSearch.prototype = Object.create(AutoCompleteSearchBase.prototype);

add_task(function *test_userContextId() {
  let searchParam = yield doSearch("test", 1);
  Assert.equal(searchParam, " user-context-id:1");
});

function doSearch(aString, aUserContextId) {
  let deferred = Promise.defer();
  let search = new AutoCompleteSearch("test");

  search.startSearch = function (aSearchString,
                                 aSearchParam,
                                 aPreviousResult,
                                 aListener) {
    unregisterAutoCompleteSearch(search);
    deferred.resolve(aSearchParam);
  };

  registerAutoCompleteSearch(search);

  let controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);

  let input = new AutoCompleteInput([ search.name ], aUserContextId);
  controller.input = input;
  controller.startSearch(aString);

  return deferred.promise;
 }

