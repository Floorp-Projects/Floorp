/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var jum = {}; Components.utils.import('resource://mozmill/modules/jum.js', jum);

var setupModule = function(module) {
  module.controller = mozmill.getBrowserController();
};

var testGetNode = function() {
  controller.open("about:support");
  controller.waitForPageLoad();

  var appbox = new elementslib.ID(controller.tabs.activeTab, "application-box");
  jum.assert(appbox.getNode().innerHTML == 'Firefox', 'correct app name');
};

const NAV_BAR             = '/id("main-window")/id("tab-view-deck")/{"flex":"1"}' +
                            '/id("navigator-toolbox")/id("nav-bar")';
const SEARCH_BAR          = NAV_BAR + '/id("search-container")/id("searchbar")';
const SEARCH_TEXTBOX      = SEARCH_BAR      + '/anon({"anonid":"searchbar-textbox"})';
const SEARCH_DROPDOWN     = SEARCH_TEXTBOX  + '/[0]/anon({"anonid":"searchbar-engine-button"})';
const SEARCH_POPUP        = SEARCH_DROPDOWN + '/anon({"anonid":"searchbar-popup"})';
const SEARCH_INPUT        = SEARCH_TEXTBOX  + '/anon({"class":"autocomplete-textbox-container"})' +
                                              '/anon({"anonid":"textbox-input-box"})' +
                                              '/anon({"anonid":"input"})';
const SEARCH_CONTEXT      = SEARCH_TEXTBOX  + '/anon({"anonid":"textbox-input-box"})' +
                                              '/anon({"anonid":"input-box-contextmenu"})';
const SEARCH_GO_BUTTON    = SEARCH_TEXTBOX  + '/anon({"class":"search-go-container"})' +
                                              '/anon({"class":"search-go-button"})';
const SEARCH_AUTOCOMPLETE = '/id("main-window")/id("mainPopupSet")/id("PopupAutoComplete")';

var testLookupExpressions = function() {
  var item;
  item = new elementslib.Lookup(controller.window.document, NAV_BAR);
  controller.click(item);
  item = new elementslib.Lookup(controller.window.document, SEARCH_BAR);
  controller.click(item);
  item = new elementslib.Lookup(controller.window.document, SEARCH_TEXTBOX);
  controller.click(item);
  item = new elementslib.Lookup(controller.window.document, SEARCH_DROPDOWN);
  controller.click(item);
  item = new elementslib.Lookup(controller.window.document, SEARCH_POPUP);
  controller.click(item);
  item = new elementslib.Lookup(controller.window.document, SEARCH_INPUT);
  controller.click(item);
  item = new elementslib.Lookup(controller.window.document, SEARCH_CONTEXT);
  controller.click(item);
  item = new elementslib.Lookup(controller.window.document, SEARCH_GO_BUTTON);
  controller.click(item);
  item = new elementslib.Lookup(controller.window.document, SEARCH_AUTOCOMPLETE);
  controller.click(item);
};
