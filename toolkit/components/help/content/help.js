# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Application Suite.
#
# The Initial Developer of the Original Code is
# Ian Oeschger.
# Portions created by the Initial Developer are Copyright (C) 2003
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#      Brant Gurganus <brantgurganus2001@cherokeescouting.org>
#      R.J. Keller <rlk@trfenv.com>
#      Steffen Wilberg <steffen.wilberg@web.de>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK ***** */

# Global Variables
var helpBrowser;
var helpWindow;
var helpSearchPanel;
var emptySearch;
var emptySearchText
var emptySearchLink
var helpTocPanel;
var helpIndexPanel;
var helpGlossaryPanel;

# Namespaces
const NC = "http://home.netscape.com/NC-rdf#";
const SN = "rdf:http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const XML = "http://www.w3.org/XML/1998/namespace#"
const MAX_LEVEL = 40; // maximum depth of recursion in search datasources.
const MAX_HISTORY_MENU_ITEMS = 6;

# Resources
const RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"]
    .getService(Components.interfaces.nsIRDFService);
const RDF_ROOT = RDF.GetResource("urn:root");
const NC_PANELLIST = RDF.GetResource(NC + "panellist");
const NC_PANELID = RDF.GetResource(NC + "panelid");
const NC_EMPTY_SEARCH_TEXT = RDF.GetResource(NC + "emptysearchtext");
const NC_EMPTY_SEARCH_LINK = RDF.GetResource(NC + "emptysearchlink");
const NC_DATASOURCES = RDF.GetResource(NC + "datasources");
const NC_SUBHEADINGS = RDF.GetResource(NC + "subheadings");
const NC_NAME = RDF.GetResource(NC + "name");
const NC_CHILD = RDF.GetResource(NC + "child");
const NC_LINK = RDF.GetResource(NC + "link");
const NC_TITLE = RDF.GetResource(NC + "title");
const NC_BASE = RDF.GetResource(NC + "base");
const NC_DEFAULTTOPIC = RDF.GetResource(NC + "defaulttopic");

const RDFCUtils = Components.classes["@mozilla.org/rdf/container-utils;1"]
    .getService().QueryInterface(Components.interfaces.nsIRDFContainerUtils);
const RDFContainer = Components.classes["@mozilla.org/rdf/container;1"]
    .getService(Components.interfaces.nsIRDFContainer);
const CONSOLE_SERVICE = Components.classes['@mozilla.org/consoleservice;1']
    .getService(Components.interfaces.nsIConsoleService);

var urnID = 0;
var RE;

var helpFileURI;
var helpFileDS;
# Set from nc:base attribute on help rdf file. It may be used for prefix
# reduction on all links within the current help set.
var helpBaseURI;

const defaultHelpFile = "chrome://help/locale/help.rdf";
# Set from nc:defaulttopic. It is used when the requested uri has no topic
# specified.
const defaultTopic = "use-help";
var searchDatasources = "rdf:null";
var searchDS = null;

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
    loadURI(uri);
}

# Initialize the Help window
function init() {
    // Cache panel references.
    helpWindow = document.getElementById("help");
    helpSearchPanel = document.getElementById("help-search-panel");
    helpTocPanel = document.getElementById("help-toc-panel");
    helpIndexPanel = document.getElementById("help-index-panel");
    helpGlossaryPanel = document.getElementById("help-glossary-panel");
    helpBrowser = document.getElementById("help-content");

    // Get the content pack, base URL, and help topic
    var helpTopic = defaultTopic;
    if ("arguments" in window
            && window.arguments[0]
            instanceof Components.interfaces.nsIDialogParamBlock) {
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
    getWebNavigation().sessionHistory = Components.classes["@mozilla.org/browser/shistory;1"]
        .createInstance(Components.interfaces.nsISHistory);
    window.XULBrowserWindow = new nsHelpStatusHandler();

    //Start the status handler.
    window.XULBrowserWindow.init();

    // Hook up UI through Progress Listener
    const interfaceRequestor = helpBrowser.docShell.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
    const webProgress = interfaceRequestor.getInterface(Components.interfaces.nsIWebProgress);

    webProgress.addProgressListener(window.XULBrowserWindow, Components.interfaces.nsIWebProgress.NOTIFY_ALL);

    //Display the Table of Contents
    showPanel("help-toc");

    // initialize the customizeDone method on the customizeable toolbar
    var toolbox = document.getElementById("help-toolbox");
    toolbox.customizeDone = ToolboxCustomizeDone;

    var toolbarset = document.getElementById("customToolbars");
    toolbox.toolbarset = toolbarset;

    // Set the text of the sidebar toolbar button to "Hide Sidebar" taken the properties file.
    // This is needed so that it says "Toggle Sidebar" in toolbar customization, but outside
    // of it, it says either "Show Sidebar" or "Hide Sidebar".
    document.getElementById("help-sidebar-button").label =
           document.getElementById("bundle_help").getString("hideSidebarLabel");
}

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
            helpWindow.setAttribute("title",
                getAttribute(helpFileDS, RDF_ROOT, NC_TITLE, ""));
            helpBaseURI = getAttribute(helpFileDS, RDF_ROOT, NC_BASE, helpBaseURI);
            defaultTopic = getAttribute(helpFileDS,
                RDF_ROOT, NC_DEFAULTTOPIC, "welcome");

            var panelDefs = helpFileDS.GetTarget(RDF_ROOT, NC_PANELLIST, true);
            RDFContainer.Init(helpFileDS, panelDefs);
            var iterator = RDFContainer.GetElements();
            while (iterator.hasMoreElements()) {
                var panelDef = iterator.getNext();
                var panelID = getAttribute(helpFileDS, panelDef, NC_PANELID,
                    null);

                var datasources = getAttribute(helpFileDS, panelDef,
                    NC_DATASOURCES, "rdf:none");
                datasources = normalizeLinks(helpBaseURI, datasources);
                // Cache Additional Datasources to Augment Search Datasources.
                if (panelID == "search") {
                    emptySearchText = getAttribute(helpFileDS, panelDef,
                        NC_EMPTY_SEARCH_TEXT, null) || "No search items found.";
                    emptySearchLink = getAttribute(helpFileDS, panelDef,
                        NC_EMPTY_SEARCH_LINK, null) || "about:blank";
                    searchDatasources = datasources;
                    // Don't try to display them yet!
                    datasources = "rdf:null";
                }

                // Cache TOC Datasources for Use by ID Lookup.
                var tree = document.getElementById("help-" + panelID + "-panel");
                loadDatabasesBlocking(datasources);
                tree.setAttribute("datasources", datasources);
            }
        } catch (e) {
            log(e + "");
        }
    }
}

function loadDatabasesBlocking(datasources) {
	var ds = datasources.split(/\s+/);
    for (var i=0; i < ds.length; ++i) {
        if (ds[i] == "rdf:null" || ds[i] == "")
            continue;
        try {
            // We need blocking here to ensure the database is loaded so
            // getLink(topic) works.
            var datasource = RDF.GetDataSourceBlocking(ds[i]);
        } catch (e) {
            log("Datasource: " + ds[i] + " was not found.");
        }
    }
}

# Prepend helpBaseURI to list of space separated links if the don't start with
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
        if (ls[i] == "") {
            continue;
        }
        if (ls[i].substr(0,7) != "chrome:" && ls[i].substr(0,4) != "rdf:") {
            ls[i] = helpBaseURI + ls[i];
        }
    }
    return ls.join(" ");
}

function getLink(ID) {
    if (!ID) {
        return null;
    }
    // Note resources are stored in fileURL#ID format.
    // We have one possible source for an ID for each datasource in the
    // composite datasource.
    // The first ID which matches is returned.
    var tocTree = document.getElementById("help-toc-panel");
    var tocDS = tocTree.database;
    if (tocDS == null) {
        return null;
    }
    var tocDatasources = tocTree.getAttribute("datasources");
    var ds = tocDatasources.split(/\s+/);
    for (var i=0; i < ds.length; ++i) {
        if (ds[i] == "rdf:null" || ds[i] == "") {
            continue;
        }
        try {
            var rdfID = ds[i] + "#" + ID;
            var resource = RDF.GetResource(rdfID);
            if (resource) {
                var link = tocDS.GetTarget(resource, NC_LINK, true);
                if (link) {
                    link = link.QueryInterface(Components.interfaces
                        .nsIRDFLiteral);
                    if (link) {
                        return link.Value;
                    } else {
                        return null;
                    }
                }
            }
        } catch (e) {
            log(rdfID + " " + e);
        }
    }
    return null;
}

# Called by contextHelp.js to determine if this window is displaying the
# requested help file.
function getHelpFileURI() {
    return helpFileURI;
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

/* copied from browser.js */
function BrowserReloadWithFlags(reloadFlags) {
    /* First, we'll try to use the session history object to reload so 
     * that framesets are handled properly. If we're in a special 
     * window (such as view-source) that has no session history, fall 
     * back on using the web navigation's reload method.
     */

    var webNav = getWebNavigation();
    try {
      var sh = webNav.sessionHistory;
      if (sh)
        webNav = sh.QueryInterface(Components.interfaces.nsIWebNavigation);
    } catch (e) {
    }

    try {
      webNav.reload(reloadFlags);
    } catch (e) {
    }
}

function reload() {
  const reloadFlags = Components.interfaces.nsIWebNavigation.LOAD_FLAGS_NONE;
  return BrowserReloadWithFlags(reloadFlags);
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
    loadURI("chrome://help/locale/firefox_welcome.xhtml");
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
          end  = ((count-index) > MAX_HISTORY_MENU_ITEMS) ? index + MAX_HISTORY_MENU_ITEMS : count;
          if ((index + 1) >= end) return false;
          for (j = index + 1; j < end; j++)
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

    onStateChange : function(aWebProgress, aRequest, aStateFlags, aStatus)
    {
      const nsIWebProgressListener = Components.interfaces.nsIWebProgressListener;

#     //Turn on and off the throbber depending on whether or not a page is loading. This is used in case
#     //someone has a Help Viewer Content Pack that gets pages off of a web server.
      if (aStateFlags & nsIWebProgressListener.STATE_START) {
        if (this.throbberElement)
          this.throbberElement.setAttribute("busy", "true");
      } else if (aStateFlags & nsIWebProgressListener.STATE_STOP) {
        if (aRequest && this.throbberElement) {
          this.throbberElement.removeAttribute("busy");
        }
      }
    },
    onProgressChange : function(aWebProgress, aRequest, aCurSelfProgress,
        aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress) {},
    onStatusChange : function(aWebProgress, aRequest, aStatus, aMessage) {},
    onSecurityChange : function(aWebProgress, aRequest, state) {},
    onLocationChange : function(aWebProgress, aRequest, aLocation) {
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

    init : function()
    {
      this.throbberElement = document.getElementById("help-throbber");
    },

    destroy : function()
    {
      //this needed to avoid memory leaks, see bug 60729
      this.throbberElement = null;
    },

    setJSStatus : function(status) {},
    setJSDefaultStatus : function(status) {},
    setOverLink : function(link) {}
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

function getMarkupDocumentViewer() {
    return helpBrowser.markupDocumentViewer;
}

# Show the selected sidebar panel
function showPanel(panelId) {
    //hide other sidebar panels and show the panel name taken in.
    helpSearchPanel.setAttribute("hidden", "true");
    helpTocPanel.setAttribute("hidden", "true");
    helpIndexPanel.setAttribute("hidden", "true");
    helpGlossaryPanel.setAttribute("hidden", "true");
    var thePanel = document.getElementById(panelId + "-panel");
    thePanel.setAttribute("hidden","false");

    //remove the selected style from the previous panel selected.
    var theButton = document.getElementById(panelId + "-btn");
    document.getElementById("help-glossary-btn").removeAttribute("selected");
    document.getElementById("help-index-btn").removeAttribute("selected");
    document.getElementById("help-search-btn").removeAttribute("selected");
    document.getElementById("help-toc-btn").removeAttribute("selected");
    //add the selected style to the correct panel.
    theButton.setAttribute("selected", "true");
}

function findParentNode(node, parentNode)
{
  if (node && node.nodeType == Node.TEXT_NODE) {
    node = node.parentNode;
  }
  while (node) {
    var nodeName = node.localName;
    if (!nodeName)
      return null;
    nodeName = nodeName.toLowerCase();
    if (nodeName == "body" || nodeName == "html" ||
        nodeName == "#document") {
      return null;
    }
    if (nodeName == parentNode)
      return node;
    node = node.parentNode;
  }
  return null;
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

# doFind - Searches the help files for what is located in findText and outputs into
#	the find search tree.
function doFind() {
    var searchTree = document.getElementById("help-search-tree");
    var findText = document.getElementById("findText");

    // clear any previous results.
    clearDatabases(searchTree.database);

    // split search string into separate terms and compile into regexp's
    RE = findText.value.split(/\s+/);
    for (var i=0; i < RE.length; ++i) {
        if (RE[i] == "")
            continue;

        RE[i] = new RegExp(RE[i], "i");
    }
    emptySearch = true;
    // search TOC
    var resultsDS = Components.classes["@mozilla.org/rdf/datasource;1?name=in-memory-datasource"]
        .createInstance(Components.interfaces.nsIRDFDataSource);
    var tree = helpTocPanel;
    var sourceDS = tree.database;
    doFindOnDatasource(resultsDS, sourceDS, RDF_ROOT, 0);

    // search glossary.
    tree = helpSearchPanel;
    sourceDS = tree.database;
    // If the glossary has never been displayed this will be null (sigh!).
    if (!sourceDS)
        sourceDS = loadCompositeDS(tree.datasources);

    doFindOnDatasource(resultsDS, sourceDS, RDF_ROOT, 0);

    // search index
    tree = helpIndexPanel;
    sourceDS = tree.database;
    //If the index has never been displayed this will be null
    if (!sourceDS)
      sourceDS = loadCompositeDS(tree.datasources);

    doFindOnDatasource(resultsDS, sourceDS, RDF_ROOT, 0);

    if (emptySearch)
        assertSearchEmpty(resultsDS);
    // Add the datasource to the search tree
    searchTree.database.AddDataSource(resultsDS);
    searchTree.builder.rebuild();
}

function doEnabling() {
    var findButton = document.getElementById("findButton");
    var findTextbox = document.getElementById("findText");
    findButton.disabled = !findTextbox.value;
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
        target = target.QueryInterface(Components.interfaces.nsIRDFResource);
        var name = sourceDS.GetTarget(target, NC_NAME, true);
        name = name.QueryInterface(Components.interfaces.nsIRDFLiteral);

        if (isMatch(name.Value)) {
            // we have found a search entry - add it to the results datasource.

            // Get URL of html for this entry.
            var link = sourceDS.GetTarget(target, NC_LINK, true);
            link = link.QueryInterface(Components.interfaces.nsIRDFLiteral);

            urnID++;
            resultsDS.Assert(RDF_ROOT,
                RDF.GetResource("http://home.netscape.com/NC-rdf#child"),
                RDF.GetResource("urn:" + urnID),
                true);
            resultsDS.Assert(RDF.GetResource("urn:" + urnID),
                 RDF.GetResource("http://home.netscape.com/NC-rdf#name"),
                 name,
                 true);
             resultsDS.Assert(RDF.GetResource("urn:" + urnID),
                RDF.GetResource("http://home.netscape.com/NC-rdf#link"),
                link,
                true);
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

function loadCompositeDS(datasources) {
# We can't search on each individual datasource's - only the aggregate
# (for each sidebar tab) has the appropriate structure.
    var compositeDS =  Components.classes["@mozilla.org/rdf/datasource;1?name=composite-datasource"]
        .createInstance(Components.interfaces.nsIRDFCompositeDataSource);

    var ds = datasources.split(/\s+/);
    for (var i=0; i < ds.length; ++i) {
        if (ds[i] == "rdf:null" || ds[i] == "") {
            continue;
        }
        try {
            // we need blocking here to ensure the database is loaded.
            var sourceDS = RDF.GetDataSourceBlocking(ds[i]);
            compositeDS.AddDataSource(sourceDS);
        } catch (e) {
            log("Datasource: " + ds[i] + " was not found.");
        }
    }
    return compositeDS;
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

# Write debug string to javascript console.
function log(aText) {
    CONSOLE_SERVICE.logStringMessage(aText);
}

# INDEX OPENING FUNCTION -- called in oncommand for index pane
#  iterate over all the items in the outliner;
#  open the ones at the top-level (i.e., expose the headings underneath
#  the letters in the list.
function expandAllIndexEntries() {
    var treeview = helpIndexPanel.view;
    var i = treeview.rowCount;
    while (i--) {
        if (!treeview.getLevel(i) && !treeview.isContainerOpen(i)) {
            treeview.toggleOpenState(i);
        }
    }
}

# toggleSidebarStatus - Toggles the visibility of the sidebar.
function toggleSidebar()
{
    var sidebar = document.getElementById("helpsidebar-box");
    var separator = document.getElementById("helpsidebar-splitter");
    var sidebarButton = document.getElementById("help-sidebar-button");

    //Use the string bundle to retrieve the text "Hide Sidebar"
    //and "Show Sidebar" from the locale directory.
    var strBundle = document.getElementById("bundle_help");
    if (sidebar.hidden) {
      sidebar.removeAttribute("hidden");
      sidebarButton.label = strBundle.getString("hideSidebarLabel");

      separator.removeAttribute("hidden");
    } else {
      sidebar.setAttribute("hidden","true");
      sidebarButton.label = strBundle.getString("showSidebarLabel");

      separator.setAttribute("hidden","true");
    }
}

// Shows the panel relative to the currently selected panel.
// Takes a boolean parameter - if true it will show the next panel, 
// otherwise it will show the previous panel.
function showRelativePanel(goForward) {
  var selectedIndex = -1;
  var sidebarBox = document.getElementById("helpsidebar-box");
  var sidebarButtons = new Array();
  for (var i = 0; i < sidebarBox.childNodes.length; i++) {
    var btn = sidebarBox.childNodes[i];
    if (btn.nodeName == "toolbarbutton") {
      if (btn.getAttribute("selected") == "true")
        selectedIndex = sidebarButtons.length;
      sidebarButtons.push(btn);
    }
  }
  if (selectedIndex == -1)
    return;
  selectedIndex += goForward ? 1 : -1;
  if (selectedIndex >= sidebarButtons.length)
    selectedIndex = 0;
  else if (selectedIndex < 0)
    selectedIndex = sidebarButtons.length - 1;
  sidebarButtons[selectedIndex].doCommand();
}

