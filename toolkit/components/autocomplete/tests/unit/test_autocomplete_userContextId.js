"use strict";


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

add_task(async function test_userContextId() {
  let searchParam = await doSearch("test", 1);
  Assert.equal(searchParam, " user-context-id:1");
});

function doSearch(aString, aUserContextId) {
  return new Promise(resolve => {
    let search = new AutoCompleteSearch("test");

    search.startSearch = function(aSearchString,
                                  aSearchParam,
                                  aPreviousResult,
                                  aListener) {
      unregisterAutoCompleteSearch(search);
      resolve(aSearchParam);
    };

    registerAutoCompleteSearch(search);

    let controller = Cc["@mozilla.org/autocomplete/controller;1"].
                     getService(Ci.nsIAutoCompleteController);

    let input = new AutoCompleteInput([ search.name ], aUserContextId);
    controller.input = input;
    controller.startSearch(aString);

  });
 }

