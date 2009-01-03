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
                Ci.nsIAutoCompleteController.STATUS_COMPLETE_MATCH);
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

var uri1 = uri("http://site.tld/1");
var uri2 = uri("http://site.tld/2");
var uri3 = uri("http://site.tld/3");
var uri4 = uri("http://site.tld/4");
var uri5 = uri("http://site.tld/5");
var uri6 = uri("http://site.tld/6");
  
var tests = [function() { ensure_tag_results([uri1, uri2, uri3], "foo"); }, 
             function() { ensure_tag_results([uri1, uri2, uri3], "Foo"); }, 
             function() { ensure_tag_results([uri1, uri2, uri3], "foO"); },
             function() { ensure_tag_results([uri4, uri5, uri6], "bar mud"); },
             function() { ensure_tag_results([uri4, uri5, uri6], "BAR MUD"); },
             function() { ensure_tag_results([uri4, uri5, uri6], "Bar Mud"); }];

/** 
 * Test bug #408221
 */
function run_test() {
  // always search in history + bookmarks, no matter what the default is
  var prefs = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefBranch);
  prefs.setIntPref("browser.urlbar.search.sources", 3);

  tagssvc.tagURI(uri1, ["Foo"]);
  tagssvc.tagURI(uri2, ["FOO"]);
  tagssvc.tagURI(uri3, ["foO"]);
  tagssvc.tagURI(uri4, ["BAR"]);
  tagssvc.tagURI(uri4, ["MUD"]);
  tagssvc.tagURI(uri5, ["bar"]);
  tagssvc.tagURI(uri5, ["mud"]);
  tagssvc.tagURI(uri6, ["baR"]);
  tagssvc.tagURI(uri6, ["muD"]);

  tests[0]();
}
