/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
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
 * The Original Code is Bug 378079 unit test code.
 *
 * The Initial Developer of the Original Code is POTI Inc.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Matt Crocker <matt@songbirdnest.com>
 *   Seth Spitzer <sspitzer@mozilla.org>
 *   Edward Lee <edward.lee@engineering.uiuc.edu>
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

var current_test = 0;

function AutoCompleteInput(aSearches) {
  this.searches = aSearches;
}
AutoCompleteInput.prototype = {
  constructor: AutoCompleteInput, 

  searches: null,
  
  minResultsForPopup: 0,
  timeout: 10,
  searchParam: "",
  textValue: "",
  disableAutoComplete: false,  
  completeDefaultIndex: false,
  
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
    QueryInterface: function(iid) {
      if (iid.equals(Ci.nsISupports) ||
          iid.equals(Ci.nsIAutoCompletePopup))
        return this;

      throw Components.results.NS_ERROR_NO_INTERFACE;
    }    
  },
    
  // nsISupports implementation
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIAutoCompleteInput))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
}

// Get tagging service
try {
  var tagssvc = Cc["@mozilla.org/browser/tagging-service;1"].
                getService(Ci.nsITaggingService);
} catch(ex) {
  do_throw("Could not get tagging service\n");
}

function ensure_tag_results(uris, searchTerm)
{
  print("Searching for '" + searchTerm + "'");
  var controller = Components.classes["@mozilla.org/autocomplete/controller;1"].
                   getService(Components.interfaces.nsIAutoCompleteController);  
  
  // Make an AutoCompleteInput that uses our searches
  // and confirms results on search complete
  var input = new AutoCompleteInput(["history"]);

  controller.input = input;

  // Search is asynchronous, so don't let the test finish immediately
  do_test_pending();

  var numSearchesStarted = 0;
  input.onSearchBegin = function() {
    numSearchesStarted++;
    do_check_eq(numSearchesStarted, 1);
  };

  input.onSearchComplete = function() {
    do_check_eq(numSearchesStarted, 1);
    do_check_eq(controller.searchStatus, 
                uris.length ?
                Ci.nsIAutoCompleteController.STATUS_COMPLETE_MATCH :
                Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH);
    do_check_eq(controller.matchCount, uris.length);
    let vals = [];
    for (var i=0; i<controller.matchCount; i++) {
      // Keep the URL for later because order of tag results is undefined
      vals.push(controller.getValueAt(i));
      do_check_eq(controller.getStyleAt(i), "tag");
    }
    // Sort the results then check if we have the right items
    vals.sort().forEach(function(val, i) do_check_eq(val, uris[i].spec))
   
    if (current_test < (tests.length - 1)) {
      current_test++;
      tests[current_test]();
    }

    do_test_finished();
  };

  controller.startSearch(searchTerm);
}

var uri1 = uri("http://site.tld/1/aaa");
var uri2 = uri("http://site.tld/2/bbb");
var uri3 = uri("http://site.tld/3/aaa");
var uri4 = uri("http://site.tld/4/bbb");
var uri5 = uri("http://site.tld/5/aaa");
var uri6 = uri("http://site.tld/6/bbb");
  
var tests = [
  function() ensure_tag_results([uri1, uri4, uri6], "foo"), 
  function() ensure_tag_results([uri1], "foo aaa"), 
  function() ensure_tag_results([uri4, uri6], "foo bbb"), 
  function() ensure_tag_results([uri2, uri4, uri5, uri6], "bar"), 
  function() ensure_tag_results([uri5], "bar aaa"), 
  function() ensure_tag_results([uri2, uri4, uri6], "bar bbb"), 
  function() ensure_tag_results([uri3, uri5, uri6], "cheese"), 
  function() ensure_tag_results([uri3, uri5], "chees aaa"), 
  function() ensure_tag_results([uri6], "chees bbb"), 
  function() ensure_tag_results([uri4, uri6], "fo bar"), 
  function() ensure_tag_results([], "fo bar aaa"), 
  function() ensure_tag_results([uri4, uri6], "fo bar bbb"), 
  function() ensure_tag_results([uri4, uri6], "ba foo"), 
  function() ensure_tag_results([], "ba foo aaa"), 
  function() ensure_tag_results([uri4, uri6], "ba foo bbb"), 
  function() ensure_tag_results([uri5, uri6], "ba chee"), 
  function() ensure_tag_results([uri5], "ba chee aaa"), 
  function() ensure_tag_results([uri6], "ba chee bbb"), 
  function() ensure_tag_results([uri5, uri6], "cheese bar"), 
  function() ensure_tag_results([uri5], "cheese bar aaa"), 
  function() ensure_tag_results([uri6], "chees bar bbb"), 
  function() ensure_tag_results([uri6], "cheese bar foo"),
  function() ensure_tag_results([], "foo bar cheese aaa"),
  function() ensure_tag_results([uri6], "foo bar cheese bbb"),
];

/**
 * Properly tags a uri adding it to bookmarks.
 *
 * @param aURI
 *        The nsIURI to tag.
 * @param aTags
 *        The tags to add.
 */
function tagURI(aURI, aTags) {
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       aURI,
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "A title");
  tagssvc.tagURI(aURI, aTags);
}

/** 
 * Test history autocomplete
 */
function run_test() {
  // always search in history + bookmarks, no matter what the default is
  var prefs = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefBranch);
  prefs.setIntPref("browser.urlbar.search.sources", 3);
  prefs.setIntPref("browser.urlbar.default.behavior", 0);

  tagURI(uri1, ["foo"]);
  tagURI(uri2, ["bar"]);
  tagURI(uri3, ["cheese"]);
  tagURI(uri4, ["foo bar"]);
  tagURI(uri5, ["bar cheese"]);
  tagURI(uri6, ["foo bar cheese"]);

  tests[0]();
}
