/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert John Churchill   <rjc@netscape.com> (Original Author)
 *   Peter Annema            <disttsc@bart.nl>
 *   Blake Ross              <blakeross@telocity.com>
 *   Ben Goodger             <ben@netscape.com>
 *   Rob Ginda               <rginda@netscape.com>
 *   Steve Lamm              <slamm@netscape.com>
 *   Samir Gehani            <sgehani@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

const ISEARCH_CONTRACTID       = "@mozilla.org/rdf/datasource;1?name=internetsearch";
const RDFSERVICE_CONTRACTID    = "@mozilla.org/rdf/rdf-service;1";
const BMARKS_CONTRACTID        = "@mozilla.org/browser/bookmarks-service;1";

const nsIBookmarksService      = Components.interfaces.nsIBookmarksService;
const nsIRDFService            = Components.interfaces.nsIRDFService;
const nsIRDFLiteral            = Components.interfaces.nsIRDFLiteral;
const nsIRDFDataSource         = Components.interfaces.nsIRDFDataSource;
const nsIRDFRemoteDataSource   = Components.interfaces.nsIRDFRemoteDataSource;
const nsIInternetSearchService = Components.interfaces.nsIInternetSearchService;

const DEBUG = false;

var debug;
if (!DEBUG)
{
  debug = function(msg) {};
}
else
{
  debug = function(msg)
  {
    dump("\t ^^^ search-panel.js: " + msg + "\n");
  };
}

var rootNode;
var textArc;
var modeArc;
var searchBundle;
var regionalBundle;
var gSearchButtonText = null;
var gStopButtonText = null;

var sidebarInitiatedSearch = false;

var RDF_observer =
{
  onAssert : function(ds, src, prop, target)
  {
    if (src == rootNode) {
      if (prop == textArc)
        rememberSearchText(target);
      else if (prop == modeArc)
        updateSearchMode();
    }
  },

  onUnassert : function(ds, src, prop, target)
  {
  },

  onChange : function(ds, src, prop, old_target, new_target)
  {
    if (src == rootNode) {
      if (prop == textArc)
        rememberSearchText(new_target);
      else if (prop == modeArc)
        updateSearchMode();
    }
  },

  onMove : function(ds, old_src, new_src, prop, target)
  {
  },

  onBeginUpdateBatch : function(ds)
  {
  },

  onEndUpdateBatch   : function(ds)
  {
  }
}

function rememberSearchText(target)
{
  if (sidebarInitiatedSearch) {
    // this avoids updating the sidebar textbox twice
    return;
  }
  if (target) {
    target = target.QueryInterface(nsIRDFLiteral).Value;
    if (target) {
      // convert plusses (+) back to spaces
      target = target.replace(/\+/g, " ");
      var textNode = document.getElementById("sidebar-search-text");

      if (target != textNode.value) {
        // category is unknown, therefore use "All Engines"
        var categoryPopup = document.getElementById( "categoryPopup" );
        var categoryList = document.getElementById( "categoryList" );
        if (categoryPopup && categoryList) {
          categoryList.selectedItem = categoryPopup.childNodes[0];
        }
      }

      textNode.value = decodeURIComponent(target);
      doEnabling();
    }
  }
  // show the results tab
  switchTab(0);
}

function getIndexToSelect(haveDefault, defaultEngine, enginePopup)
{
  if (haveDefault)
  {
    // iterate over constrained engine list for a default engine match
    for (var i = 0; i < enginePopup.childNodes.length; ++i)
    {
      if (enginePopup.childNodes[i].id == defaultEngine)
      {
        // found match with default engine!
        return i;
      }
    }
  }

  return 0; // default to first engine at index '0'
}

function getMinVer()
{
  var minVerStr = nsPreferences.
    copyUnicharPref("browser.search.basic.min_ver");
  var minVer = parseFloat(minVerStr);
  if (isNaN(minVer))
    minVer = 0; // no pref or not a number so default to min ver 0.0

  debug("Float value of minVer = " + minVer);
  return minVer;
}

/** 
 * Constrain the list of engines to only those that
 * contain ver >= browser.search.basic.min_ver pref 
 * value to be displayed in the basic popup list of 
 * engines.
 */
function constrainBasicList()
{
  debug("constrainBasicList");

  var basicEngineMenu = document.getElementById("basicEngineMenu");
  if (!basicEngineMenu)
  {
    debug("Ack!  basicEngineList is empty!");
    return;
  }

  var basicEngines = basicEngineMenu.childNodes[1];
  var len = basicEngines.childNodes.length;
  var currDesc;
  var currVer;
  var haveDefault = false;

  debug("Found " + len + " sherlock plugins.");
  var defaultEngine = nsPreferences.
    copyUnicharPref("browser.search.defaultengine");

  // we constrain the basic list to all engines with ver >= minVer
  var minVer = getMinVer();

  for (var i = 0; i < len; ++i)
  {
    currDesc = basicEngines.childNodes[i].getAttribute("desc");
    debug("Engine[" + i + "] = " + currDesc);

    // make sure we leave the default engine (check if we already have 
    // the default to avoid duplicates)
    if (basicEngines.childNodes[i].id == defaultEngine && !haveDefault)
    {
      haveDefault = true;
    }
    else
    {
      // remove if not a basic engine
      currVer = basicEngines.childNodes[i].getAttribute("ver");
      if (!currVer || isNaN(parseFloat(currVer)))
      {
        // missing version attr or not a number: default to ver 1.0
        currVer = 1;
      }
      debug("Version of " + (currDesc ? currDesc : "<unknown>") + 
            " is: " + currVer);

      if (parseFloat(currVer) < minVer)
      {
        try
        {
          basicEngines.removeChild(basicEngines.childNodes[i]);
          --i;
          --len;
        }
        catch (ex)
        {
          debug("Exception: Couldn't remove " + currDesc +
                " from engine list!");
        }
      }
    }
  }

  // mark selected engine
  var selected = getIndexToSelect(haveDefault, defaultEngine, basicEngines);
  basicEngineMenu.selectedItem = basicEngines.childNodes[selected];
}

function updateSearchMode()
{
  debug("updateSearchMode");

  var searchMode = nsPreferences.getIntPref("browser.search.mode");
  var categoryBox = document.getElementById("categoryBox");
  var basicBox = document.getElementById("basicBox");

  debug("search mode is " + searchMode);
  if (searchMode == 0)
  {
    categoryBox.setAttribute("collapsed", "true");
    basicBox.removeAttribute("collapsed");
    switchTab(0);

    constrainBasicList();
  }
  else
  {
    categoryBox.removeAttribute("collapsed");
    basicBox.setAttribute("collapsed", "true");
    switchTab(1);
  }

  return searchMode;
}

function readRDFString(aDS,aRes,aProp) {
  var n = aDS.GetTarget(aRes, aProp, true);
  if (n)
    return n.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
  return null;
}


function ensureDefaultEnginePrefs(aRDF,aDS) 
{
  var prefbranch = Components.classes["@mozilla.org/preferences-service;1"]
                             .getService(Components.interfaces.nsIPrefBranch);
  var defaultName = prefbranch.getComplexValue("browser.search.defaultenginename",
                                               Components.interfaces.nsIPrefLocalizedString).data;
  var kNC_Root = aRDF.GetResource("NC:SearchEngineRoot");
  var kNC_child = aRDF.GetResource("http://home.netscape.com/NC-rdf#child");
  var kNC_Name = aRDF.GetResource("http://home.netscape.com/NC-rdf#Name");
          
  var arcs = aDS.GetTargets(kNC_Root, kNC_child, true);
  while (arcs.hasMoreElements()) {
    var engineRes = arcs.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
    var name = readRDFString(aDS, engineRes, kNC_Name);
    if (name == defaultName)
      prefbranch.setCharPref("browser.search.defaultengine", engineRes.Value);
  }
}

function AskChangeDefaultEngine(aSelectedEngine)
{
  const kDontAskAgainPref  = "browser.search.set_default.dont_ask_again";
  const kDefaultEnginePref = "browser.search.defaultengine";

  // don't prompt user if selection is same as old default engine
  var oldDefault = nsPreferences.copyUnicharPref(kDefaultEnginePref);
  if (aSelectedEngine.getAttribute("value") == oldDefault)
    return;

  // check "don't ask again" pref
  var dontAskAgain = nsPreferences.getBoolPref(kDontAskAgainPref, false);
  var change = false; // should we change the default engine?

  // if "don't ask again" pref was set
  if (dontAskAgain)
  {
    change = true;
  }
  else
  {
    // prompt user if we should change their default engine
    var promptSvc = Components.
                      classes["@mozilla.org/embedcomp/prompt-service;1"].
                      getService(Components.interfaces.nsIPromptService);
    if (!promptSvc)
      return;
    var title = searchBundle.getString("changeEngineTitle"); 
    var dontAskAgainMsg = searchBundle.getString("dontAskAgainMsg"); 
    var engineName = aSelectedEngine.getAttribute("label");
    if (!engineName || engineName == "")
      engineName = searchBundle.getString("thisEngine");
    var changeEngineMsg = searchBundle.stringBundle.formatStringFromName(
                          "changeEngineMsg", [engineName], 1); 

    var checkbox = {value:0};
    var choice = promptSvc.confirmEx(window, title, changeEngineMsg, 
                   (promptSvc.BUTTON_TITLE_YES * promptSvc.BUTTON_POS_0) + 
                   (promptSvc.BUTTON_TITLE_NO * promptSvc.BUTTON_POS_1),
                   null, null, null, dontAskAgainMsg, checkbox);
    if (choice == 0)
      change = true;

    // store "don't ask again" pref from checkbox value (if changed)
    debug("dontAskAgain: " + dontAskAgain);
    debug("checkbox.value: " + checkbox.value);
    if (checkbox.value != dontAskAgain)
      nsPreferences.setBoolPref(kDontAskAgainPref, checkbox.value);
  }

  // if confirmed true, i.e., change default engine, then set pref
  if (change)
    nsPreferences.setUnicharPref(kDefaultEnginePref, aSelectedEngine.value);

  disableNavButtons();
}

function disableNavButtons()
{
  var nextButton = document.getElementById("next-results");
  var prevButton = document.getElementById("prev-results");
  if (nextButton && nextButton.getAttribute("disabled") != "true")
    nextButton.setAttribute("disabled", "true");
  if (prevButton && prevButton.getAttribute("disabled") != "true")
    prevButton.setAttribute("disabled", "true");
}

function ensureSearchPref()
{
  var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                      .getService(Components.interfaces.nsIRDFService);
  var prefbranch = Components.classes["@mozilla.org/preferences-service;1"]
                             .getService(Components.interfaces.nsIPrefBranch);
  var ds = rdf.GetDataSource("rdf:internetsearch");
  var kNC_Name = rdf.GetResource("http://home.netscape.com/NC-rdf#Name");
  var defaultEngine;

  try {
    defaultEngine = prefbranch.getCharPref("browser.search.defaultengine");
  } catch(ex) {
    ensureDefaultEnginePrefs(rdf, ds);
    defaultEngine = prefbranch.getCharPref("browser.search.defaultengine");
  }
}

// Initialize the Search panel:
// 1) init the category list
// 2) load the search engines associated with this category
// 3) initialize the checked state of said engines
function SearchPanelStartup()
{
  searchBundle = document.getElementById("searchBundle");

  var rdf  = Components.classes[RDFSERVICE_CONTRACTID].getService(nsIRDFService);
  rootNode = rdf.GetResource("NC:LastSearchRoot", true);
  textArc  = rdf.GetResource("http://home.netscape.com/NC-rdf#LastText", true);
  modeArc  = rdf.GetResource("http://home.netscape.com/NC-rdf#SearchMode", true);
  
  ensureSearchPref()
  
  var resultList = document.getElementById("resultList");
  resultList.database.AddObserver(RDF_observer);

  var categoryList = document.getElementById("categoryList");
  var internetSearch = Components.classes[ISEARCH_CONTRACTID].getService(nsIInternetSearchService);
  var catDS = internetSearch.GetCategoryDataSource();
  if (catDS) {
    catDS = catDS.QueryInterface(nsIRDFDataSource);
    categoryList.database.AddDataSource(catDS);
    categoryList.builder.rebuild();
    var engineList = document.getElementById("searchengines");
    engineList.database.AddDataSource(catDS);
    engineList.builder.rebuild();
    engineList.addEventListener("CheckboxStateChange", saveEngine, false);
  }

  // try and determine last category name used
  var lastCategoryName = nsPreferences.copyUnicharPref("browser.search.last_search_category", "");
  if (lastCategoryName) {
    // strip off the prefix if necessary
    var prefix = "NC:SearchCategory?category=";
    if (lastCategoryName.indexOf(prefix) == 0)
      lastCategoryName = lastCategoryName.substr(prefix.length);
  }

  // select the appropriate category
  var categoryPopup = document.getElementById("categoryPopup");
  var found = false;
  for (var i = 0; i < categoryPopup.childNodes.length; ++i) {
    if (lastCategoryName == "" &&
        categoryPopup.childNodes[i].getAttribute("value") == "NC:SearchEngineRoot" ||
        categoryPopup.childNodes[i].getAttribute("id") == lastCategoryName)
    {
      categoryList.selectedItem = categoryPopup.childNodes[i];
      found = true;
      break;
    }
  }
  if (!found)
    categoryList.selectedItem = categoryPopup.childNodes[0];

  if (!lastCategoryName)
    lastCategoryName = "NC:SearchEngineRoot";
  else if (lastCategoryName != "NC:SearchEngineRoot")
    lastCategoryName = "NC:SearchCategory?category=" + lastCategoryName;

  var treeNode = document.getElementById("searchengines");
  treeNode.setAttribute("ref", lastCategoryName);

  loadEngines(lastCategoryName);

  // if we have search results, show them, otherwise show engines
  if (updateSearchMode() != 0)
  {
    if (haveSearchResults())
      switchTab(0);
    else
      switchTab(1);
  }
}

function haveSearchResults()
{
  var ds = document.getElementById("resultList").database;
  if (!ds)
    return false;

  var rdf = Components.classes[RDFSERVICE_CONTRACTID].getService(nsIRDFService);
  var source = rdf.GetResource("NC:LastSearchRoot", true);
  var childProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#LastText", true);
  var target = ds.GetTarget(source, childProperty, true);
  if (target) {
    target = target.QueryInterface(nsIRDFLiteral).Value;
    if (target) {
      // convert plusses (+) back to spaces
      target = target.replace(/\+/g, " ");
      var textNode = document.getElementById("sidebar-search-text");
      textNode.value = decodeURI(target);
      return true;
    }
  }
  return false;
}

function getNumEngines()
{
  var listbox = document.getElementById("searchengines");
  return listbox.getElementsByTagName("listitem").length;
}

function chooseCategory(aNode)
{
  var category = !aNode.id ? "NC:SearchEngineRoot" :
                             "NC:SearchCategory?category=" + aNode.getAttribute("id");
  nsPreferences.setUnicharPref("browser.search.last_search_category", category);

  var treeNode = document.getElementById("searchengines");
  treeNode.setAttribute("ref", category);

  loadEngines(category);
}

function saveEngine(event)
{
  var rdf = Components.classes[RDFSERVICE_CONTRACTID].getService(nsIRDFService);
  var localStore = rdf.GetDataSource("rdf:local-store");
  if (!localStore)
    return;

  var checkedProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#checked", true);
  var engines = event.currentTarget;
  var categorySRC = rdf.GetResource(engines.ref, true);

  var itemNode = event.target;
  var engineSRC = rdf.GetResource(itemNode.id, true);

  if (itemNode.checked)
    localStore.Assert(categorySRC, checkedProperty, engineSRC, true);
  else
    localStore.Unassert(categorySRC, checkedProperty, engineSRC, true);

  // save changes; flush out the localstore
  try {
    var flushableStore = localStore.QueryInterface(nsIRDFRemoteDataSource);
    flushableStore.Flush();
  }
  catch (ex) {}
}

// initialize the appropriate engine list, and the checked state of the engines
function loadEngines(aCategory)
{
  var rdf = Components.classes[RDFSERVICE_CONTRACTID].getService(nsIRDFService);
  var localStore = rdf.GetDataSource("rdf:local-store");
  if (localStore) {
    var engineBox = document.getElementById("searchengines");
    var numEngines = engineBox.childNodes.length;
    var checkedProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#checked", true);
    var categorySRC = rdf.GetResource(aCategory, true);
    for (var x = 0; x < numEngines; ++x) {
      var listitemNode = engineBox.childNodes[x];
      if (listitemNode.localName == "listitem") {
        var engineURI = listitemNode.getAttribute("id");
        var engineSRC = rdf.GetResource(engineURI, true);
  
        var hasAssertion = localStore.HasAssertion(categorySRC, checkedProperty, engineSRC, true);
        if (hasAssertion)
          listitemNode.checked = true;
      }
    }
  }
}

function SearchPanelShutdown()
{
  var tree = document.getElementById("resultList");
  tree.database.RemoveObserver(RDF_observer);
}

function doStop()
{
  if (!gSearchButtonText)
    gSearchButtonText = searchBundle.getString("searchButtonText");

  var searchButtonNode = document.getElementById("searchButton");
  searchButtonNode.removeAttribute("stop");
  searchButtonNode.setAttribute("label", gSearchButtonText);

  // should stop button press also stop the load of the page in the browser? I think so.
  var progressNode = parent.document.getElementById("statusbar-icon");
  if (progressNode)
    progressNode.setAttribute("mode", "normal");

  // stop any network connections
  var internetSearchService = Components.classes[ISEARCH_CONTRACTID].getService(nsIInternetSearchService);
  internetSearchService.Stop();

  // show appropriate column(s)
  var navWindow = getNavigatorWindow(false);
  var resultsList = navWindow ? navWindow.content.document.getElementById("resultsList") : null;
  if (!resultsList)
    return;

  var searchURL = resultsList.getAttribute("ref");
  if (!searchURL)
    return;

  var internetSearch = internetSearchService.QueryInterface(nsIRDFDataSource);

  // get various services
  var rdf = Components.classes[RDFSERVICE_CONTRACTID].getService(nsIRDFService);

  var searchResource       = rdf.GetResource(searchURL, true);
  var priceProperty        = rdf.GetResource("http://home.netscape.com/NC-rdf#Price", true);
  var availabilityProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#Availability", true);
  var relevanceProperty    = rdf.GetResource("http://home.netscape.com/NC-rdf#Relevance", true);
  var dateProperty         = rdf.GetResource("http://home.netscape.com/NC-rdf#Date", true);
  var trueProperty         = rdf.GetLiteral("true");
  var hasPriceFlag         = internetSearch.HasAssertion(searchResource, priceProperty, trueProperty, true);
  var hasAvailabilityFlag  = internetSearch.HasAssertion(searchResource, availabilityProperty, trueProperty, true);
  var hasRelevanceFlag     = internetSearch.HasAssertion(searchResource, relevanceProperty, trueProperty, true);
  var hasDateFlag          = internetSearch.HasAssertion(searchResource, dateProperty, trueProperty, true);
  var colNode;
  var sortSetFlag = false;

  if (hasPriceFlag) {
    colNode = navWindow.content.document.getElementById("PriceColumn");
    if (colNode) {
      colNode.removeAttribute("hidden");
      if (!sortSetFlag) {
        top.content.setInitialSort(colNode, "ascending");
        sortSetFlag = true;
      }
    }
  }

  if (hasAvailabilityFlag) {
    colNode = navWindow.content.document.getElementById("AvailabilityColumn");
    if (colNode)
      colNode.removeAttribute("hidden");
  }

  if (hasDateFlag) {
    colNode = navWindow.content.document.getElementById("DateColumn");
    if (colNode)
      colNode.removeAttribute("hidden");
  }

  if (hasRelevanceFlag) {
    colNode = navWindow.content.document.getElementById("RelevanceColumn");
    if (colNode) {
      colNode.removeAttribute("hidden");
      if (!sortSetFlag) {
        navWindow.content.setInitialSort(colNode, "descending");
        sortSetFlag = true;
      }
    }
  }

  if (!sortSetFlag) {
    colNode = navWindow.content.document.getElementById("PageRankColumn");
    if (colNode)
      navWindow.content.setInitialSort(colNode, "ascending");
  }

  switchTab(0);
}

function doSearch()
{
  var navWindow = getNavigatorWindow(true);
  if (navWindow.content)
    onNavWindowLoad();
  else
    navWindow.addEventListener("load", onNavWindowLoad, false);
}

function onNavWindowLoad() {
  var navWindow = getNavigatorWindow(true);

  // hide various columns
  if (navWindow && "content" in navWindow && "isMozillaSearchWindow" in navWindow.content) {
    colNode = navWindow.content.document.getElementById("RelevanceColumn");
    if (colNode)
      colNode.setAttribute("hidden", "true");

    colNode = navWindow.content.document.getElementById("PriceColumn");
    if (colNode)
      colNode.setAttribute("hidden", "true");

    colNode = navWindow.content.document.getElementById("AvailabilityColumn");
    if (colNode)
      colNode.setAttribute("hidden", "true");
  }

  // get user text to find
  var textNode = document.getElementById("sidebar-search-text");

  if (!textNode.value) {
    alert(searchBundle.getString("enterstringandlocation"));
    return;
  }

  var searchMode = nsPreferences.getIntPref("browser.search.mode", 0);
  var engineURIs = [];
  if (searchMode > 0) {
    var foundEngine = false;
    var itemNode;
    var engineBox = document.getElementById("searchengines");

    // in advanced search mode, get selected search engines
    // (for the current search category)
    for (var x = 0; x < engineBox.childNodes.length; ++x) {
      itemNode = engineBox.childNodes[x];

      if (itemNode.localName == "listitem" && itemNode.checked) {
        var engineURI = itemNode.id;

        if (engineURI) {
          engineURIs[engineURIs.length] = engineURI;
          foundEngine = true;
        }
      }
    }

    if (!foundEngine) {
      if (getNumEngines() == 1) {
        // only one engine in this category, check it
        itemNode = engineBox.firstChild;
        engineURIs[engineURIs.length] = itemNode.id;
      }
      else {
        for (var i = 0; i < engineBox.childNodes.length; ++i) {
          itemNode = engineBox.childNodes[i];
          var theID = itemNode.id;
          if (theID.indexOf("google.src") != -1) {
            engineURIs[engineURIs.length] = theID;
            foundEngine = true;
            break;
          }
        }

        if (!foundEngine) {
          alert(searchBundle.getString("enterstringandlocation"));
          return;
        }
      }
    }
  }
  else
  {
    var basicEngines = document.getElementById("basicEngineMenu");
    engineURIs[0] = basicEngines.selectedItem.id;
    debug("basic mode URI = " + engineURIs[0]);
  }

  if (!gStopButtonText)
    gStopButtonText = searchBundle.getString("stopButtonText");

  var searchButtonNode = document.getElementById("searchButton");
  searchButtonNode.setAttribute("stop", "true");
  searchButtonNode.setAttribute("label", gStopButtonText);

  var progressNode = top.document.getElementById("statusbar-icon");
  if (progressNode)
    progressNode.setAttribute("mode", "undetermined");

  // run the search
  OpenSearch(textNode.value, engineURIs);
  switchTab(0);
}

function checkSearchProgress()
{
  var activeSearchFlag = false;
  var navWindow = getNavigatorWindow(false);

  if (navWindow) {
    var resultsList = navWindow.content.document.getElementById("resultsList");
    if (resultsList) {
      var treeref = resultsList.getAttribute("ref");
      var ds = resultsList.database;
      if (ds && treeref) {
        try {
          var rdf = Components.classes[RDFSERVICE_CONTRACTID].getService(nsIRDFService);
          var source = rdf.GetResource(treeref, true);
          var loadingProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#loading", true);
          var target = ds.GetTarget(source, loadingProperty, true);
          if (target)
            target = target.QueryInterface(nsIRDFLiteral).Value;
          activeSearchFlag = target == "true" ? true : false;
        }
        catch (ex) {
          activeSearchFlag = false;
        }
      }
    }
  }

  if (activeSearchFlag)
    setTimeout("checkSearchProgress()", 1000);
  else
    doStop();

  return activeSearchFlag;
}

function sidebarOpenURL(listitem)
{
  var id = listitem.id;
  if (!id)
    return;

  // rjc: add support for anonymous resources; if the node has
  // a "#URL" property, use it, otherwise default to using the id
  try {
    var ds = document.getElementById("resultList").database;
    if (ds) {
      var rdf = Components.classes[RDFSERVICE_CONTRACTID].getService(nsIRDFService);
      var src = rdf.GetResource(id, true);
      var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
      var target = ds.GetTarget(src, prop, true);
      if (target) {
        target = target.QueryInterface(nsIRDFLiteral).Value;
        if (target)
          id = target;
      }
    }
  } catch (ex) {
  }

  // mark result as visited
  listitem.setAttribute("visited", "true");
  
  loadURLInContent(id);
}

function OpenSearch(aSearchStr, engineURIs)
{
  var searchEngineURI = nsPreferences.copyUnicharPref("browser.search.defaultengine", null);
  var defaultSearchURL = nsPreferences.getLocalizedUnicharPref("browser.search.defaulturl", null);

  if (!defaultSearchURL) {
    regionalBundle = document.getElementById("regionalBundle");
    defaultSearchURL = regionalBundle.getString("defaultSearchURL");
  }

  var searchDS = Components.classes[ISEARCH_CONTRACTID].getService(nsIInternetSearchService);

  var escapedSearchStr = encodeURIComponent(aSearchStr);
  sidebarInitiatedSearch = true;
  searchDS.RememberLastSearchText(escapedSearchStr);
  sidebarInitiatedSearch = false;

  var gURL;
  try {
    if (!engineURIs || engineURIs.length <= 1) {
      // not called from sidebar or only one engine selected
      if (engineURIs && engineURIs.length == 1) {
        searchEngineURI = engineURIs[0];
        gURL = "internetsearch:engine=" + searchEngineURI + "&text=" + escapedSearchStr;
      }

      if (!searchEngineURI)
        searchEngineURI = regionalBundle.getString("defaultSearchURL");

      // look up the correct search URL format for the given engine
      try {
        var whichButtons = new Object;
        whichButtons.value = 0;
        var searchURL = searchDS.GetInternetSearchURL(searchEngineURI, escapedSearchStr, 0, 0, whichButtons);
        doNavButtonEnabling(whichButtons.value, searchDS, 0);
      }
      catch (ex) {
        debug("Exception when calling GetInternetSearchURL: " + ex);
        searchURL = "";
      }

      defaultSearchURL = searchURL ? searchURL : defaultSearchURL + escapedSearchStr;
      if (!searchURL)
        gURL = "";

      // load the results page of selected or default engine in the content area
      if (defaultSearchURL)
        loadURLInContent(defaultSearchURL);
    }
    else {
      // multiple providers
      searchURL = "";
      for (var i = 0; i < engineURIs.length; ++i) {
        searchURL += !searchURL ? "internetsearch:" : "&";
        searchURL += "engine=" + engineURIs[i];
      }
      searchURL += ("&text=" + escapedSearchStr);
      gURL = searchURL;
      loadURLInContent(encodeURI("chrome://communicator/content/search/internetresults.xul?" + searchURL));
    }
  }
  catch (ex) {
    debug("Exception: " + ex);
  }

  setTimeout("checkSearchProgress()", 1000);
}

function saveSearch()
{
  var ds = document.getElementById("resultList").database;
  if (!ds)
    return;

  var lastSearchURI = "";
  var lastSearchText = "";

  var rdf = Components.classes[RDFSERVICE_CONTRACTID].getService(nsIRDFService);
  // look for last search URI
  var source = rdf.GetResource("NC:LastSearchRoot", true);
  var childProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#ref", true);
  var target = ds.GetTarget(source, childProperty, true);
  if (target) {
    target = target.QueryInterface(nsIRDFLiteral).Value;
    if (target)
      lastSearchURI = target;
  }

  // look for last search text
  childProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#LastText", true);
  target = ds.GetTarget(source, childProperty, true);
  if (target) {
    target = target.QueryInterface(nsIRDFLiteral).Value;
    if (target) {
      // convert plusses (+) back to spaces
      target = target.replace(/\+/g, " ");
      lastSearchText = decodeURIComponent(target);
    }
  }

  if (!lastSearchURI)
    return;

  // rjc says: if lastSearchText is empty/null, that's still OK, synthesize the text
  if (!lastSearchText) {
    lastSearchText = lastSearchURI;
    var siteOffset = lastSearchText.indexOf("://");
    if (siteOffset > 0) {
      siteOffset += 3;
      var endOffset = lastSearchText.indexOf("/", siteOffset);
      if (endOffset > 0)
        lastSearchText = lastSearchText.substr(0, endOffset+1);
    }
  }

  var searchTitle = searchBundle.stringBundle.formatStringFromName(
                      "searchTitle", [lastSearchText], 1);

  var bmks = Components.classes[BMARKS_CONTRACTID].getService(nsIBookmarksService);
  bmks.addBookmarkImmediately(lastSearchURI, searchTitle, bmks.BOOKMARK_SEARCH_TYPE, null);
}

var gPageNumber = 0;
/**
 * showMoreResults
 *
 * Run a query to show the next/previous page of search results for the
 * current search term.
 * 
 * @param direction : -1 => previous
 *                     1 => next
 */
function showMoreResults(direction)
{
  // XXX check if we are in basic search mode

  // get search engine
  var engine = document.getElementById("basicEngineMenu").selectedItem;
  var engineURI = engine.id;

  // get search term
  var searchTerm = document.getElementById("sidebar-search-text").value;
  searchTerm = encodeURIComponent(searchTerm);

  // change page number
  if (direction > 0)
    ++gPageNumber;
  else
    --gPageNumber;

  // get qualified URL
  var searchService = Components.classes[ISEARCH_CONTRACTID].
                        getService(nsIInternetSearchService);
  var whichButtons = new Object;
  whichButtons.value = 0;
  var searchURL = searchService.GetInternetSearchURL(engineURI, searchTerm, 
                    direction, gPageNumber, whichButtons);

  doNavButtonEnabling(whichButtons.value, searchService, gPageNumber);

  // load URL in navigator
  loadURLInContent(searchURL);
}

function doNavButtonEnabling(whichButtons, searchService, pageNumber)
{
  var nextButton = document.getElementById("next-results");
  var nextDisabled = nextButton.getAttribute("disabled");
  var prevButton = document.getElementById("prev-results");
  var prevDisabled = prevButton.getAttribute("disabled");

  if (whichButtons & searchService.kHaveNext)
  {
    if (nextDisabled)
      nextButton.removeAttribute("disabled");
  }
  else 
  {
    if (!nextDisabled)
      nextButton.setAttribute("disabled", "true");
  }
  
  if ((pageNumber > 0) && (whichButtons & searchService.kHavePrev))
  {
    if (prevDisabled)
      prevButton.removeAttribute("disabled");
  }
  else 
  {
    if (!prevDisabled)
      prevButton.setAttribute("disabled", "true");
  }
}

function doCustomize()
{
  //Switching from Edit Categories back to All Engines then launching customize window
  var category = document.getElementById("categoryList");
  category.selectedIndex = 0;
  window.openDialog("chrome://communicator/content/search/search-editor.xul", "internetsearch:editor", "centerscreen,chrome,resizable");
}

function loadURLInContent(url)
{
  var navigatorWindow = getNavigatorWindow(true);
  navigatorWindow.loadURI(url);
}

function search_getBrowserURL()
{
  return nsPreferences.copyUnicharPref("browser.chromeURL", "chrome://navigator/content/navigator.xul");
}

function doEnabling()
{
  var searchButton = document.getElementById("searchButton");
  var sidebarSearchText = document.getElementById("sidebar-search-text");
  searchButton.disabled = !sidebarSearchText.value;
}

//Step up the dom until getting the desired node.
function getItemNode(aNode,nodeName)
{
  var node = aNode;
  while (node.localName != nodeName) {
    node = node.parentNode;
  }
  return node ? node : null;
}

function getArcValueForID(aArc, aID)
{
  var val = null;

  try
  {
    var ds = document.getElementById("resultList").database;
    if (ds)
    {
      var rdf = Components.classes[RDFSERVICE_CONTRACTID].
                  getService(nsIRDFService);
      var src = rdf.GetResource(aID, true);
      var prop = rdf.GetResource(
                  "http://home.netscape.com/NC-rdf#"+aArc, true);
      val = ds.GetTarget(src, prop, true);        
      if (val)
        val = val.QueryInterface(nsIRDFLiteral).Value;
    }
  } 
  catch (ex)
  {
    dump("Exception: no value for " + aArc + "!\t" + ex + "\n");
    val = null;
  }

  return val;
}

//Fill in tooltip in teh search results panel
function FillInDescTooltip(tipElement)
{
  var retValue = false;
 
  //Get the Name of the listitem for first item in the tooltip
  var nodeLabel = tipElement.getAttribute("label");
  var nodeID = tipElement.id;
 
  //Query RDF to get URL of listitem
  if (nodeID)
    var url = getArcValueForID("URL", nodeID);

  //Fill in the the text nodes
  //collapse them if there is not a node
  if (nodeLabel || url) {
    var tooltipTitle = document.getElementById("titleText");
    var tooltipUrl = document.getElementById("urlText"); 
    if (nodeLabel) {
      if (tooltipTitle.getAttribute("hidden") == "true")
        tooltipTitle.removeAttribute("hidden");
      tooltipTitle.setAttribute("value",nodeLabel);
    } 
    else  {
      tooltipTitle.setAttribute("hidden", "true");
    }
    if (url) {
      if (tooltipUrl.getAttribute("hidden") == "true")
        tooltipUrl.removeAttribute("hidden");
      if (url.length > 100)
        url = url.substr(0,100) + "...";
      tooltipUrl.setAttribute("value",url);
    }
    else {
      tooltipUrl.setAttribute("hidden", "true");
    }
    retValue = true;
  }
  return retValue;
}

var nsResultDNDObserver = 
{
  onDragStart: function(aEvent, aXferData, aDragAction)
  {
    var node = getItemNode(aEvent.target, "listitem");
    var URL = getArcValueForID("URL", node.id);
    var title = getArcValueForID("Name", node.id);
    var htmlString = "<a href=\"" + URL + "\">" + title + "</a>";
    var urlString = URL + "\n" + title;

    aXferData.data = new TransferData();
    aXferData.data.addDataForFlavour("text/x-moz-url", URL);
    aXferData.data.addDataForFlavour("text/unicode", urlString);
    aXferData.data.addDataForFlavour("text/html", htmlString); 
  }
};

function HandleResultDragGesture(aEvent)
{
  nsDragAndDrop.startDrag(aEvent, nsResultDNDObserver);
  return true;  
}

