/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *    Robert John Churchill   <rjc@netscape.com> (Original Author)
 *    Peter Annema            <disttsc@bart.nl>
 *    Blake Ross              <blakeross@telocity.com>
 *    Ben Goodger             <ben@netscape.com>
 *    Rob Ginda               <rginda@netscape.com>
 *    Steve Lamm              <slamm@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const WMEDIATOR_CONTRACTID     = "@mozilla.org/rdf/datasource;1?name=window-mediator";
const ISEARCH_CONTRACTID       = "@mozilla.org/rdf/datasource;1?name=internetsearch";
const RDFSERVICE_CONTRACTID    = "@mozilla.org/rdf/rdf-service;1";
const BMARKS_CONTRACTID        = "@mozilla.org/browser/bookmarks-service;1";

const nsIBookmarksService      = Components.interfaces.nsIBookmarksService;
const nsIWindowMediator        = Components.interfaces.nsIWindowMediator;
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

  beginUpdateBatch : function(ds)
  {
  },

  endUpdateBatch   : function(ds)
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
      target = target.replace(/+/g, " ");
      var textNode = document.getElementById("sidebar-search-text");

      if (target != textNode.value) {
        // category is unknown, therefore use "All Engines"
        var categoryPopup = document.getElementById( "categoryPopup" );
        var categoryList = document.getElementById( "categoryList" );
        if (categoryPopup && categoryList) {
          categoryList.selectedItem = categoryPopup.childNodes[0];
        }
      }

      textNode.value = unescape(target);
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
}


function ensureDefaultEnginePrefs(aRDF,aDS) 
   {

    mPrefs = Components.classes["@mozilla.org/preferences;1"].getService(Components.interfaces.nsIPrefBranch);
    var defaultName = mPrefs.getComplexValue("browser.search.defaultenginename" , Components.interfaces.nsIPrefLocalizedString);
    kNC_Root = aRDF.GetResource("NC:SearchEngineRoot");
    kNC_child = aRDF.GetResource("http://home.netscape.com/NC-rdf#child");
    kNC_Name = aRDF.GetResource("http://home.netscape.com/NC-rdf#Name");
          
    var arcs = aDS.GetTargets(kNC_Root, kNC_child, true);
    while (arcs.hasMoreElements()) {
        var engineRes = arcs.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
        var name = readRDFString(aDS, engineRes, kNC_Name);
        if (name == defaultName)
		   mPrefs.setCharPref("browser.search.defaultengine", engineRes.Value);
    }
  }


function ensureSearchPref() {
   

   var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);
   mPrefs = Components.classes["@mozilla.org/preferences;1"].getService(Components.interfaces.nsIPrefBranch);
   var ds = rdf.GetDataSource("rdf:internetsearch");
   kNC_Name = rdf.GetResource("http://home.netscape.com/NC-rdf#Name");
   try {
        defaultEngine = mPrefs.getCharPref("browser.search.defaultengine");
        } catch(ex) {
            ensureDefaultEnginePrefs(rdf, ds);
            defaultEngine = mPrefs.getCharPref("browser.search.defaultengine");
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
  
  var tree = document.getElementById("Tree");
  tree.database.AddObserver(RDF_observer);

  var categoryList = document.getElementById("categoryList");
  var internetSearch = Components.classes[ISEARCH_CONTRACTID].getService(nsIInternetSearchService);
  var catDS = internetSearch.GetCategoryDataSource();
  if (catDS) {
    catDS = catDS.QueryInterface(nsIRDFDataSource);
    categoryList.database.AddDataSource(catDS);
    var ref = categoryList.getAttribute("ref");
    if (ref)
      categoryList.setAttribute("ref", ref);
    var engineTree = document.getElementById("searchengines");
    engineTree.database.AddDataSource(catDS);
    ref = engineTree.getAttribute("ref");
    if (ref)
      engineTree.setAttribute("ref", ref);
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
  focusTextBox();
}

function haveSearchResults()
{
  var ds = document.getElementById("Tree").database;
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
      target = target.replace(/+/g, " ");
      var textNode = document.getElementById("sidebar-search-text");
      textNode.value = unescape(target);
      return true;
    }
  }
  return false;
}

function getNumEngines()
{
  var treeChildrenNode = document.getElementById("engineKids");
  return treeChildrenNode.childNodes.length;
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

function saveEngines()
{
  var categoryList = document.getElementById("categoryList");
  var category = categoryList.selectedItem.getAttribute("id");
  if (category)
    category = "NC:SearchCategory?category=" + category;
  else
    category = "NC:SearchEngineRoot";

  var rdf = Components.classes[RDFSERVICE_CONTRACTID].getService(nsIRDFService);
  var localStore = rdf.GetDataSource("rdf:local-store");
  if (!localStore)
    return;

  var engineBox = document.getElementById("engineKids");

  var checkedProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#checked", true);
  var categorySRC = rdf.GetResource(category, true);

  for (var x = 0; x < engineBox.childNodes.length; ++x) {
    var treeitemNode = engineBox.childNodes[x];

    var checkboxNode = treeitemNode.firstChild.firstChild.firstChild;
    if (checkboxNode) {
      var engineURI = treeitemNode.getAttribute("id");
      var engineSRC = rdf.GetResource(engineURI, true);

      if (checkboxNode.checked)
        localStore.Assert(categorySRC, checkedProperty, engineSRC, true);
      else
        localStore.Unassert(categorySRC, checkedProperty, engineSRC, true);
    }
  }

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
    var engineBox = document.getElementById("engineKids");
    var numEngines = engineBox.childNodes.length;
    var checkedProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#checked", true);
    var categorySRC = rdf.GetResource(aCategory, true);
    for (var x = 0; x < numEngines; ++x) {
      var treeitemNode = engineBox.childNodes[x];
      var engineURI = treeitemNode.getAttribute("id");
      var engineSRC = rdf.GetResource(engineURI, true);

      var checkboxNode = treeitemNode.firstChild.firstChild.firstChild;
      if (checkboxNode) {
        var hasAssertion = localStore.HasAssertion(categorySRC, checkedProperty, engineSRC, true);
        if (hasAssertion)
          checkboxNode.checked = true;
      }
    }
  }
}

function focusTextBox()
{
	var textBox = document.getElementById("sidebar-search-text");
	textBox.focus();
}

function SearchPanelShutdown()
{
  var tree = document.getElementById("Tree");
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
  var resultsTree = navWindow ? navWindow._content.document.getElementById("internetresultstree") : null;
  if (!resultsTree)
    return;

  var searchURL = resultsTree.getAttribute("ref");
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
    colNode = navWindow._content.document.getElementById("PriceColumn");
    if (colNode) {
      colNode.removeAttribute("hidden");
      if (!sortSetFlag) {
        top._content.setInitialSort(colNode, "ascending");
        sortSetFlag = true;
      }
    }
  }

  if (hasAvailabilityFlag) {
    colNode = navWindow._content.document.getElementById("AvailabilityColumn");
    if (colNode)
      colNode.removeAttribute("hidden");
  }

  if (hasDateFlag) {
    colNode = navWindow._content.document.getElementById("DateColumn");
    if (colNode)
      colNode.removeAttribute("hidden");
  }

  if (hasRelevanceFlag) {
    colNode = navWindow._content.document.getElementById("RelevanceColumn");
    if (colNode) {
      colNode.removeAttribute("hidden");
      if (!sortSetFlag) {
        navWindow._content.setInitialSort(colNode, "descending");
        sortSetFlag = true;
      }
    }
  }

  if (!sortSetFlag) {
    colNode = navWindow._content.document.getElementById("PageRankColumn");
    if (colNode)
      navWindow._content.setInitialSort(colNode, "ascending");
  }

  switchTab(0);
}

function doSearch()
{
  var searchMode = nsPreferences.getIntPref("browser.search.mode", 0);

  // hide various columns
  var navWindow = getNavigatorWindow(false);
  if (navWindow && "_content" in navWindow && "isMozillaSearchWindow" in navWindow._content) {
    colNode = navWindow._content.document.getElementById("RelevanceColumn");
    if (colNode)
      colNode.setAttribute("hidden", "true");

    colNode = navWindow._content.document.getElementById("PriceColumn");
    if (colNode)
      colNode.setAttribute("hidden", "true");

    colNode = navWindow._content.document.getElementById("AvailabilityColumn");
    if (colNode)
      colNode.setAttribute("hidden", "true");
  }

  // get user text to find
  var textNode = document.getElementById("sidebar-search-text");

  if (!textNode.value) {
    alert(searchBundle.getString("enterstringandlocation"));
    return;
  }

  if (searchMode > 0) {
    var foundEngine = false;
    var engineURIs  = [];
    var treeitemNode;
    var engineBox = document.getElementById("engineKids");

    // in advanced search mode, get selected search engines
    // (for the current search category)
    for (var x = 0; x < engineBox.childNodes.length; x++) {
      treeitemNode = engineBox.childNodes[x];

      if (treeitemNode) {
        var checkboxNode = treeitemNode.firstChild.firstChild.firstChild;

        if (checkboxNode && checkboxNode.checked) {
          var engineURI = treeitemNode.id;

          if (engineURI) {
            engineURIs[engineURIs.length] = engineURI;
            foundEngine = true;
          }
        }
      }
    }

    if (!foundEngine) {
      if (getNumEngines() == 1) {
        // only one engine in this category, check it
        treeitemNode = engineBox.firstChild;
        engineURIs[engineURIs.length] = treeitemNode.id;
      }
      else {
        for (var i = 0; i < engineBox.childNodes.length; ++i) {
          treeitemNode = engineBox.childNodes[i];
          var theID = treeitemNode.id;
          if (theID.indexOf("NetscapeSearch.src") != -1) {
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
    var engineURIs = [];
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
    var resultsTree = navWindow._content.document.getElementById("internetresultstree");
    if (resultsTree) {
      var treeref = resultsTree.getAttribute("ref");
      var ds = resultsTree.database;
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

function sidebarOpenURL(treeitem)
{
  if (treeitem.getAttribute("container") == "true" ||
      treeitem.getAttribute("type") == "http://home.netscape.com/NC-rdf#BookmarkSeparator")   {
    return;
  }

  var id = treeitem.id;
  if (!id)
    return;

  // rjc: add support for anonymous resources; if the node has
  // a "#URL" property, use it, otherwise default to using the id
  try {
    var ds = document.getElementById("Tree").database;
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

  var escapedSearchStr = escape(aSearchStr);
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
        var searchURL = searchDS.GetInternetSearchURL(searchEngineURI, escapedSearchStr);
      }
      catch (ex) {
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
      loadURLInContent("chrome://communicator/content/search/internetresults.xul?" + escape(searchURL));
    }
  }
  catch (ex) {}

  setTimeout("checkSearchProgress()", 1000);
}

function switchTab(aPageIndex)
{
  var deck = document.getElementById("advancedDeck");
  deck.setAttribute("index", aPageIndex);

  // decide whether to show/hide/enable/disable save search query button
  if (aPageIndex != 0)
    return;

  var ds = document.getElementById("Tree").database;
  if (!ds)
    return;

  var haveSearchRef = false;

  var rdf = Components.classes[RDFSERVICE_CONTRACTID].getService(nsIRDFService);
  // look for last search URI
  var source = rdf.GetResource("NC:LastSearchRoot", true);
  var childProperty = rdf.GetResource("http://home.netscape.com/NC-rdf#ref", true);
  var target = ds.GetTarget(source, childProperty, true);
  if (target) {
    target = target.QueryInterface(nsIRDFLiteral).Value;
    if (target)
      haveSearchRef = true;
  }

  var saveQueryButton = document.getElementById("saveQueryButton");
  saveQueryButton.disabled = !haveSearchRef;
}

function saveSearch()
{
  var ds = document.getElementById("Tree").database;
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
      target = target.replace(/+/g, " ");
      lastSearchText = unescape(target);
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

  var searchTitle = "Search: '" + lastSearchText + "'";  // using " + gSites;

  var bmks = Components.classes[BMARKS_CONTRACTID].getService(nsIBookmarksService);
  bmks.AddBookmark(lastSearchURI, searchTitle, bmks.BOOKMARK_SEARCH_TYPE, null);
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

// retrieves the most recent navigator window
function getNavigatorWindow(aOpenFlag)
{
  var navigatorWindow;

  // if this is a browser window, just use it
  if ("document" in top) {
    var possibleNavigator = top.document.getElementById("main-window");
    if (possibleNavigator &&
        possibleNavigator.getAttribute("windowtype") == "navigator:browser")
      navigatorWindow = top;
  }

  // if not, get the most recently used browser window
  if (!navigatorWindow) {
    var wm = Components.classes[WMEDIATOR_CONTRACTID].getService(nsIWindowMediator);
    navigatorWindow = wm.getMostRecentWindow("navigator:browser");
  }

  // if no browser window available and it's ok to open a new one, do so
  if (!navigatorWindow && aOpenFlag) {
    var navigatorChromeURL = search_getBrowserURL();
    navigatorWindow = openDialog(navigatorChromeURL, "_blank", "chrome,all,dialog=no");
  }
  return navigatorWindow;
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
