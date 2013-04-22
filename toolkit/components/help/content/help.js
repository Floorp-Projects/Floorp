# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Global Variables
var helpBrowser;
var helpSearchPanel;
var emptySearch;
var emptySearchText;
var emptySearchLink = "about:blank";
var helpTocPanel;
var helpIndexPanel;
var helpGlossaryPanel;
var strBundle;
var gTocDSList = "";

# Namespaces
const NC = "http://home.netscape.com/NC-rdf#";
const MAX_LEVEL = 40; // maximum depth of recursion in search datasources.
const MAX_HISTORY_MENU_ITEMS = 6;

# ifdef logic ripped from toolkit/components/help/content/platformClasses.css
#ifdef XP_WIN
const platform = "win";
#else
#ifdef XP_MACOSX
const platform = "mac";
#else
#ifdef XP_OS2
const platform = "os2";
#else
const platform = "unix";
#endif
#endif
#endif

# Resources
const RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"]
    .getService(Components.interfaces.nsIRDFService);
const RDF_ROOT = RDF.GetResource("urn:root");
const NC_PANELLIST = RDF.GetResource(NC + "panellist");
const NC_PANELID = RDF.GetResource(NC + "panelid");
const NC_EMPTY_SEARCH_TEXT = RDF.GetResource(NC + "emptysearchtext");
const NC_EMPTY_SEARCH_LINK = RDF.GetResource(NC + "emptysearchlink");
const NC_DATASOURCES = RDF.GetResource(NC + "datasources");
const NC_PLATFORM = RDF.GetResource(NC + "platform");
const NC_SUBHEADINGS = RDF.GetResource(NC + "subheadings");
const NC_NAME = RDF.GetResource(NC + "name");
const NC_CHILD = RDF.GetResource(NC + "child");
const NC_LINK = RDF.GetResource(NC + "link");
const NC_TITLE = RDF.GetResource(NC + "title");
const NC_BASE = RDF.GetResource(NC + "base");
const NC_DEFAULTTOPIC = RDF.GetResource(NC + "defaulttopic");

var RDFContainer =
   Components.classes["@mozilla.org/rdf/container;1"]
             .createInstance(Components.interfaces.nsIRDFContainer);
const CONSOLE_SERVICE = Components.classes['@mozilla.org/consoleservice;1']
    .getService(Components.interfaces.nsIConsoleService);

var RE;

var helpFileURI;
var helpFileDS;
# Set from nc:base attribute on help rdf file. It may be used for prefix
# reduction on all links within the current help set.
var helpBaseURI;

/* defaultTopic is either set
   1. in the openHelp() call, passed as an argument to the Help window and
      evaluated in init(), or
   2. in nc:defaulttopic in the content pack (e.g. firebirdhelp.rdf),
      evaluated in loadHelpRDF(), or
   3. "welcome" as a fallback, specified in loadHelpRDF() as well;
      displayTopic() then uses defaultTopic because topic is null. */
var defaultTopic;

const NSRESULT_RDF_SYNTAX_ERROR = 0x804e03f7;

# This function is called by dialogs/windows that want to display
# context-sensitive help
# These dialogs/windows should include the script
# chrome://help/content/contextHelp.js
function displayTopic(topic) {
    // Get the page to open.
    var uri = getLink(topic);
    // Use default topic if specified topic is not found.
    if (!uri) {
        uri = getLink(defaultTopic);
    }
    // Load the page.
    if (uri)
      loadURI(uri);
}

# Initialize the Help window
function init() {
  // Cache panel references.
  helpSearchPanel = document.getElementById("help-search-panel");
  helpTocPanel = document.getElementById("help-toc-panel");
  helpIndexPanel = document.getElementById("help-index-panel");
  helpGlossaryPanel = document.getElementById("help-glossary-panel");
  helpBrowser = document.getElementById("help-content");

  // Turn off unnecessary features for security
  helpBrowser.docShell.allowJavascript = false;
  helpBrowser.docShell.allowPlugins = false;
  helpBrowser.docShell.allowSubframes = false;
  helpBrowser.docShell.allowMetaRedirects = false;

  strBundle = document.getElementById("bundle_help");
  emptySearchText = strBundle.getString("emptySearchText");

  // Get the content pack, base URL, and help topic
  var helpTopic = defaultTopic;
  if ("arguments" in window && 
       window.arguments[0] instanceof Components.interfaces.nsIDialogParamBlock) {
    helpFileURI = window.arguments[0].GetString(0);
    // trailing "/" included.
    helpBaseURI = helpFileURI.substring(0, helpFileURI.lastIndexOf("/")+1);
    helpTopic = window.arguments[0].GetString(1);
  }

  loadHelpRDF();
  displayTopic(helpTopic);

  // Move to Center of Screen
  const width = document.documentElement.getAttribute("width");
  const height = document.documentElement.getAttribute("height");
  window.moveTo((screen.availWidth - width) / 2, (screen.availHeight - height) / 2);

  // Initialize history.
  getWebNavigation().sessionHistory = 
    Components.classes["@mozilla.org/browser/shistory;1"]
              .createInstance(Components.interfaces.nsISHistory);
  window.XULBrowserWindow = new nsHelpStatusHandler();

  //Start the status handler.
  window.XULBrowserWindow.init();

  // Hook up UI through Progress Listener
  const interfaceRequestor = helpBrowser.docShell.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
  const webProgress = interfaceRequestor.getInterface(Components.interfaces.nsIWebProgress);

  webProgress.addProgressListener(window.XULBrowserWindow, Components.interfaces.nsIWebProgress.NOTIFY_ALL);

  var searchBox = document.getElementById("findText");
  searchBox.clickSelectsAll = getBoolPref("browser.urlbar.clickSelectsAll", true);

  setTimeout(focusSearch, 0);
}

function showSearchSidebar() {
  // if you tab too quickly, you end up with stuck focus, revert focus to the searchbar
  var searchTree = document.getElementById("help-toc-panel");
  if (searchTree.treeBoxObject.focused) {
    focusSearch();
  }

  var tableOfContents = document.getElementById("help-toc-sidebar");
  tableOfContents.setAttribute("hidden", "true");

  var sidebar = document.getElementById("help-search-sidebar");
  sidebar.removeAttribute("hidden");
}

function hideSearchSidebar(aEvent) {
  // if we're focused in the search results, focus content
  var searchTree = document.getElementById("help-search-tree");
  if (searchTree.treeBoxObject.focused) {
    content.focus();
  }

  var sidebar = document.getElementById("help-search-sidebar");
  sidebar.setAttribute("hidden", "true");

  var tableOfContents = document.getElementById("help-toc-sidebar");
  tableOfContents.removeAttribute("hidden");
}

# loadHelpRDF
# Parse the provided help content pack RDF file, and use it to
# populate the datasources attached to the trees in the viewer.
# Filter out any information not applicable to the user's platform.
function loadHelpRDF() {
  if (!helpFileDS) {
    try {
      helpFileDS = RDF.GetDataSourceBlocking(helpFileURI);
    } catch (e if (e.result == NSRESULT_RDF_SYNTAX_ERROR)) {
      log("Help file: " + helpFileURI + " contains a syntax error.");
    } catch (e) {
      log("Help file: " + helpFileURI + " was not found.");
    }

    try {
      document.title = getAttribute(helpFileDS, RDF_ROOT, NC_TITLE, "");
      helpBaseURI = getAttribute(helpFileDS, RDF_ROOT, NC_BASE, helpBaseURI);
      // if there's no nc:defaulttopic in the content pack, set "welcome"
      // as the default topic
      defaultTopic = getAttribute(helpFileDS,
        RDF_ROOT, NC_DEFAULTTOPIC, "welcome");

      var panelDefs = helpFileDS.GetTarget(RDF_ROOT, NC_PANELLIST, true);
      RDFContainer.Init(helpFileDS, panelDefs);
      var iterator = RDFContainer.GetElements();
      while (iterator.hasMoreElements()) {
        var panelDef = iterator.getNext();

        var panelID        = getAttribute(helpFileDS, panelDef, NC_PANELID, null);
        var datasources    = getAttribute(helpFileDS, panelDef, NC_DATASOURCES, "");
        var panelPlatforms = getAttribute(helpFileDS, panelDef, NC_PLATFORM, null);

        if (panelPlatforms && panelPlatforms.split(/\s+/).indexOf(platform) == -1)
          continue; // ignore datasources for other platforms

        // empty datasources are valid on search panel definitions
        // convert them to "rdf:null" which can be filtered and ignored
        if (!datasources)
          datasources = "rdf:null";

        datasources = normalizeLinks(helpBaseURI, datasources);

        var datasourceArray = datasources.split(/\s+/)
                                         .filter(function(x) { return x != "rdf:null"; })
                                         .map(RDF.GetDataSourceBlocking);

        // Cache Additional Datasources to Augment Search Datasources.
        if (panelID == "search") {
          emptySearchText = getAttribute(helpFileDS, panelDef, NC_EMPTY_SEARCH_TEXT, emptySearchText);
          emptySearchLink = getAttribute(helpFileDS, panelDef, NC_EMPTY_SEARCH_LINK, emptySearchLink);

          datasourceArray.forEach(helpSearchPanel.database.AddDataSource,
                                  helpSearchPanel.database);
          if (!panelPlatforms)
            filterDatasourceByPlatform(helpSearchPanel.database);

          continue; // to next panel definition
        }

        // cache toc datasources list for use in getLink()
        if (panelID == "toc")
          gTocDSList += " " + datasources;

        var tree = document.getElementById("help-" + panelID + "-panel");

        // add each datasource to the current tree
        datasourceArray.forEach(tree.database.AddDataSource,
                                tree.database);

        // filter and display the current tree
        if (!panelPlatforms)
          filterDatasourceByPlatform(tree.database);
        tree.builder.rebuild();
      }
    } catch (e) {
      log(e + "");
    }
  }
}

# filterDatasourceByPlatform
# Remove statements for other platforms from a datasource.
function filterDatasourceByPlatform(aDatasource) {
  filterNodeByPlatform(aDatasource, RDF_ROOT, 0);
}

# filterNodeByPlatform
# Remove statements for other platforms from the provided datasource.
function filterNodeByPlatform(aDatasource, aCurrentResource, aCurrentLevel) {
  if (aCurrentLevel > MAX_LEVEL) {
     log("Datasources over " + MAX_LEVEL + " levels deep are unsupported.");
     return;
  }

  // get the subheadings under aCurrentResource and filter them
  var nodes = aDatasource.GetTargets(aCurrentResource, NC_SUBHEADINGS, true);
  while (nodes.hasMoreElements()) {
    var node = nodes.getNext();
    node = node.QueryInterface(Components.interfaces.nsIRDFResource);
    // should we test for rdf:Seq here?  see also doFindOnDatasource
    filterSeqByPlatform(aDatasource, node, aCurrentLevel+1);
  }
}

# filterSeqByPlatform
# Go through the children of aNode, if any, removing statements applicable
# only on other platforms.
function filterSeqByPlatform(aDatasource, aNode, aCurrentLevel) {
  // get nc:subheading children into an enumerator
  var RDFC = Components.classes["@mozilla.org/rdf/container;1"]
                       .createInstance(Components.interfaces.nsIRDFContainer);
  RDFC.Init(aDatasource, aNode);
  var targets = RDFC.GetElements();

  // process items in the rdf:Seq
  while (targets.hasMoreElements()) {
    var currentTarget = targets.getNext();

    // find out on which platforms this node is meaningful
    var nodePlatforms = getAttribute(aDatasource,
                                     currentTarget.QueryInterface(Components.interfaces.nsIRDFResource),
                                     NC_PLATFORM,
                                     platform);

    if (nodePlatforms.split(/\s+/).indexOf(platform) == -1) { // node is for another platform
      var currentNode = currentTarget.QueryInterface(Components.interfaces.nsIRDFNode);
      // "false" because we don't want to renumber elements in the container
      RDFC.RemoveElement(currentNode, false);

      // move to next node - ignore the children, because 1) they might be
      // needed elsewhere and 2) nodes not connected to RDF_ROOT are ignored
      continue;
    }

    // filter any children
    filterNodeByPlatform(aDatasource, currentTarget, aCurrentLevel+1);
  }
}

# Prepend helpBaseURI to list of space separated links if they don't start with
# "chrome:"
function normalizeLinks(helpBaseURI, links) {
  if (!helpBaseURI) {
    return links;
  }
  var ls = links.split(/\s+/);
  if (ls.length == 0) {
    return links;
  }
  for (var i=0; i < ls.length; ++i) {
    if (ls[i] == "")
      continue;
      
    if (ls[i].substr(0,7) != "chrome:" && ls[i].substr(0,4) != "rdf:")
      ls[i] = helpBaseURI + ls[i];
  }
  return ls.join(" ");
}

function getLink(ID) {
    if (!ID)
      return null;

    var tocDS = document.getElementById("help-toc-panel").database;
    if (!tocDS)
      return null;

    // URIs include both the ID part and the base file name,
    // so we need to check for a matching ID in each datasource
    var tocDSArray = gTocDSList.split(/\s+/)
                               .filter(function(x) { return x != "rdf:null"; });

    for (var i = 0; i < tocDSArray.length; i++) {
      var resource = RDF.GetResource(tocDSArray[i] + "#" + ID);
      var link = tocDS.GetTarget(resource, NC_LINK, true);
      if (!link)  // no such rdf:ID found
        continue;
      return link.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
    }
    return null;
}

# Called by contextHelp.js to determine if this window is displaying the
# requested help file.
function getHelpFileURI() {
    return helpFileURI;
}

function getBrowser() {
  return helpBrowser;
}

function getWebNavigation() {
  try {
    return helpBrowser.webNavigation;
  } catch (e)
  {
    return null;
  }
}

function loadURI(uri) {
    if (uri.substr(0,7) != "chrome:") {
        uri = helpBaseURI + uri;
    }
    getWebNavigation().loadURI(uri, Components.interfaces.nsIWebNavigation.LOAD_FLAGS_NONE,
        null, null, null);
}

function goBack() {
  try
  {
    getWebNavigation().goBack();
  } catch (e)
  {
  }
}

function goForward() {
    try
    {
      getWebNavigation().goForward();
    } catch(e)
    {
    }
}

function goHome() {
    // Load "Welcome" page
    displayTopic(defaultTopic);
}

function print() {
    try {
        _content.print();
    } catch (e) {
    }
}

function FillHistoryMenu(aParent, aMenu)
  {
    // Remove old entries if any
    deleteHistoryItems(aParent);

    var sessionHistory = getWebNavigation().sessionHistory;

    var count = sessionHistory.count;
    var index = sessionHistory.index;
    var end;
    var j;
    var entry;

    switch (aMenu)
      {
        case "back":
          end = (index > MAX_HISTORY_MENU_ITEMS) ? index - MAX_HISTORY_MENU_ITEMS : 0;
          if ((index - 1) < end) return false;
          for (j = index - 1; j >= end; j--)
            {
              entry = sessionHistory.getEntryAtIndex(j, false);
              if (entry)
                createMenuItem(aParent, j, entry.title);
            }
          break;
        case "forward":
          end  = ((count-index) > MAX_HISTORY_MENU_ITEMS) ? index + MAX_HISTORY_MENU_ITEMS : count - 1;
          if ((index + 1) > end) return false;
          for (j = index + 1; j <= end; j++)
            {
              entry = sessionHistory.getEntryAtIndex(j, false);
              if (entry)
                createMenuItem(aParent, j, entry.title);
            }
          break;
      }
    return true;
  }

function createMenuItem( aParent, aIndex, aLabel)
  {
    var menuitem = document.createElement( "menuitem" );
    menuitem.setAttribute( "label", aLabel );
    menuitem.setAttribute( "index", aIndex );
    aParent.appendChild( menuitem );
  }

function deleteHistoryItems(aParent)
{
  var children = aParent.childNodes;
  for (var i = children.length - 1; i >= 0; --i)
    {
      var index = children[i].getAttribute("index");
      if (index)
        aParent.removeChild(children[i]);
    }
}

function createBackMenu(event) {
    return FillHistoryMenu(event.target, "back");
}

function createForwardMenu(event) {
    return FillHistoryMenu(event.target, "forward");
}

function gotoHistoryIndex(aEvent) {
    var index = aEvent.target.getAttribute("index");
    if (!index) {
        return false;
    }
    try {
        getWebNavigation().gotoIndex(index);
    } catch(ex) {
        return false;
    }
    return true;
}

function nsHelpStatusHandler() {
  this.init();
}

nsHelpStatusHandler.prototype = {

    onStateChange : function(aWebProgress, aRequest, aStateFlags, aStatus) {},
    onProgressChange : function(aWebProgress, aRequest, aCurSelfProgress,
        aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress) {},
    onStatusChange : function(aWebProgress, aRequest, aStatus, aMessage) {},
    onSecurityChange : function(aWebProgress, aRequest, state) {},
    onLocationChange : function(aWebProgress, aRequest, aLocation, aFlags) {
        UpdateBackForwardButtons();
    },
    QueryInterface : function(aIID) {
        if (aIID.equals(Components.interfaces.nsIWebProgressListener) ||
                aIID.equals(Components.interfaces.nsISupportsWeakReference) ||
                aIID.equals(Components.interfaces.nsIXULBrowserWindow) ||
                aIID.equals(Components.interfaces.nsISupports)) {
            return this;
        }
        throw Components.results.NS_NOINTERFACE;
    },

    init : function() {},

    destroy : function() {},

    setJSStatus : function(status) {},
    setOverLink : function(link, context) {},
    onBeforeLinkTraversal: function(originalTarget, linkURI, linkNode, isAppTab) {}
}

function UpdateBackForwardButtons() {
    var backBroadcaster = document.getElementById("canGoBack");
    var forwardBroadcaster = document.getElementById("canGoForward");
    var webNavigation = getWebNavigation();

    // Avoid setting attributes on broadcasters if the value hasn't changed!
    // Remember, guys, setting attributes on elements is expensive!  They
    // get inherited into anonymous content, broadcast to other widgets, etc.!
    // Don't do it if the value hasn't changed! - dwh

    var backDisabled = (backBroadcaster.getAttribute("disabled") == "true");
    var forwardDisabled = (forwardBroadcaster.getAttribute("disabled") == "true");

    if (backDisabled == webNavigation.canGoBack) {
      if (backDisabled)
        backBroadcaster.removeAttribute("disabled");
      else
        backBroadcaster.setAttribute("disabled", true);
    }

    if (forwardDisabled == webNavigation.canGoForward) {
      if (forwardDisabled)
        forwardBroadcaster.removeAttribute("disabled");
      else
        forwardBroadcaster.setAttribute("disabled", true);
    }
}

function onselect_loadURI(tree) {
    try {
        var resource = tree.view.getResourceAtIndex(tree.currentIndex);
        var link = tree.database.GetTarget(resource, NC_LINK, true);
        if (link) {
            link = link.QueryInterface(Components.interfaces.nsIRDFLiteral);
            loadURI(link.Value);
        }
    } catch (e) {
    }// when switching between tabs a spurious row number is returned.
}

function focusSearch() {
  var searchBox = document.getElementById("findText");
  searchBox.focus();
}

# doFind - Searches the help files for what is located in findText and outputs into
#        the find search tree.
function doFind() {
    if (document.getElementById("help-search-sidebar").hidden)
      showSearchSidebar();

    var searchTree = document.getElementById("help-search-tree");
    var findText = document.getElementById("findText");

    // clear any previous results.
    clearDatabases(searchTree.database);

    // if the search string is empty or contains only whitespace, purge the results tree and return
    RE = findText.value.match(/\S+/g);
    if (!RE) {
      searchTree.builder.rebuild();
      hideSearchSidebar();
      return;
    }

    // compile the search string, which has already been split up above, into regexps
    for (var i=0; i < RE.length; ++i) {
      RE[i] = new RegExp(RE[i], "i");
    }
    emptySearch = true;

    // search TOC
    var resultsDS = Components.classes["@mozilla.org/rdf/datasource;1?name=in-memory-datasource"]
        .createInstance(Components.interfaces.nsIRDFDataSource);
    var sourceDS = helpTocPanel.database;
    doFindOnDatasource(resultsDS, sourceDS, RDF_ROOT, 0);

    // search glossary.
    sourceDS = helpGlossaryPanel.database;
    doFindOnDatasource(resultsDS, sourceDS, RDF_ROOT, 0);

    // search index
    sourceDS = helpIndexPanel.database;
    doFindOnDatasource(resultsDS, sourceDS, RDF_ROOT, 0);

    // search additional search datasources
    sourceDS = helpSearchPanel.database;
    doFindOnDatasource(resultsDS, sourceDS, RDF_ROOT, 0);

    if (emptySearch)
        assertSearchEmpty(resultsDS);
    // Add the datasource to the search tree
    searchTree.database.AddDataSource(resultsDS);
    searchTree.builder.rebuild();
}

function clearDatabases(compositeDataSource) {
    var enumDS = compositeDataSource.GetDataSources()
    while (enumDS.hasMoreElements()) {
        var ds = enumDS.getNext();
        compositeDataSource.RemoveDataSource(ds);
    }
}

function doFindOnDatasource(resultsDS, sourceDS, resource, level) {
    if (level > MAX_LEVEL) {
        try {
            log("Recursive reference to resource: " + resource.Value + ".");
        } catch (e) {
            log("Recursive reference to unknown resource.");
        }
        return;
    }
    // find all SUBHEADING children of current resource.
    var targets = sourceDS.GetTargets(resource, NC_SUBHEADINGS, true);
    while (targets.hasMoreElements()) {
        var target = targets.getNext();
        target = target.QueryInterface(Components.interfaces.nsIRDFResource);
        // The first child of a rdf:subheading should (must) be a rdf:seq.
        // Should we test for a SEQ here?
        doFindOnSeq(resultsDS, sourceDS, target, level+1);
    }
}

function doFindOnSeq(resultsDS, sourceDS, resource, level) {
    // load up an RDFContainer so we can access the contents of the current
    // rdf:seq.
    RDFContainer.Init(sourceDS, resource);
    var targets = RDFContainer.GetElements();
    while (targets.hasMoreElements()) {
        var target = targets.getNext();
        var link = sourceDS.GetTarget(target, NC_LINK, true);
        var name = sourceDS.GetTarget(target, NC_NAME, true);

        if (link &&
            name instanceof Components.interfaces.nsIRDFLiteral &&
            isMatch(name.Value)) {
            // we have found a search entry - add it to the results datasource.
            var urn = RDF.GetAnonymousResource();
            resultsDS.Assert(urn, NC_NAME, name, true);
            resultsDS.Assert(urn, NC_LINK, link, true);
            resultsDS.Assert(RDF_ROOT, NC_CHILD, urn, true);

            emptySearch = false;
        }
        // process any nested rdf:seq elements.
        doFindOnDatasource(resultsDS, sourceDS, target, level+1);
    }
}

function assertSearchEmpty(resultsDS) {
    var resSearchEmpty = RDF.GetResource("urn:emptySearch");
        resultsDS.Assert(RDF_ROOT,
            NC_CHILD,
                resSearchEmpty,
                true);
        resultsDS.Assert(resSearchEmpty,
            NC_NAME,
                RDF.GetLiteral(emptySearchText),
                true);
        resultsDS.Assert(resSearchEmpty,
                NC_LINK,
                RDF.GetLiteral(emptySearchLink),
                true);
}

function isMatch(text) {
    for (var i=0; i < RE.length; ++i ) {
        if (!RE[i].test(text)) {
            return false;
        }
    }
    return true;
}

function getAttribute(datasource, resource, attributeResourceName,
        defaultValue) {
    var literal = datasource.GetTarget(resource, attributeResourceName, true);
    if (!literal) {
        return defaultValue;
    }
    return getLiteralValue(literal, defaultValue);
}

function getLiteralValue(literal, defaultValue) {
    if (literal) {
        literal = literal.QueryInterface(Components.interfaces.nsIRDFLiteral);
        if (literal) {
            return literal.Value;
        }
    }
    if (defaultValue) {
        return defaultValue;
    }
    return null;
}

# Write debug string to error console.
function log(aText) {
    CONSOLE_SERVICE.logStringMessage(aText);
}

function getBoolPref (aPrefname, aDefault)
{
  try { 
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    return pref.getBoolPref(aPrefname);
  }
  catch(e) {
    return aDefault;
  }
}

# getXulWin - Returns the current Help window as a nsIXULWindow.
function getXulWin()
{
  window.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
  var webnav = window.getInterface(Components.interfaces.nsIWebNavigation);
  var dsti = webnav.QueryInterface(Components.interfaces.nsIDocShellTreeItem);
  var treeowner = dsti.treeOwner;
  var ifreq = treeowner.QueryInterface(Components.interfaces.nsIInterfaceRequestor);

  return ifreq.getInterface(Components.interfaces.nsIXULWindow);
}

# toggleZLevel - Toggles whether or not the window will always appear on top. Because
#   alwaysRaised is not supported on an OS other than Windows, this code will not
#   appear in those builds.
#
#   element - The DOM node that persists the checked state.
#ifdef XP_WIN
#define HELP_ALWAYS_RAISED_TOGGLE
#endif
#ifdef HELP_ALWAYS_RAISED_TOGGLE
function toggleZLevel(element)
{
  var xulwin = getXulWin();
  
  // Now we can flip the zLevel, and set the attribute so that it persists correctly
  if (xulwin.zLevel > xulwin.normalZ) {
    xulwin.zLevel = xulwin.normalZ;
    element.setAttribute("checked", "false");
  } else {
    xulwin.zLevel = xulwin.raisedZ;
    element.setAttribute("checked", "true");
  }
}
#endif
