/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Stephen Lamm            <slamm@netscape.com>
 *                 Robert John Churchill   <rjc@netscape.com>
 */

/*
 * No magic constructor behaviour, as is de rigeur for XPCOM.
 * If you must perform some initialization, and it could possibly fail (even
 * due to an out-of-memory condition), you should use an Init method, which
 * can convey failure appropriately (thrown exception in JS,
 * NS_FAILED(nsresult) return in C++).
 *
 * In JS, you can actually cheat, because a thrown exception will cause the
 * CreateInstance call to fail in turn, but not all languages are so lucky.
 * (Though ANSI C++ provides exceptions, they are verboten in Mozilla code
 * for portability reasons -- and even when you're building completely
 * platform-specific code, you can't throw across an XPCOM method boundary.)
 */

const DEBUG = false; /* set to false to suppress debug messages */
const PANELS_RDF_FILE  = "UPnls"; /* directory services property to find panels.rdf */

const SIDEBAR_CONTRACTID   = "@mozilla.org/sidebar;1";
const SIDEBAR_CID      = Components.ID("{22117140-9c6e-11d3-aaf1-00805f8a4905}");
const CONTAINER_CONTRACTID = "@mozilla.org/rdf/container;1";
const DIR_SERV_CONTRACTID  = "@mozilla.org/file/directory_service;1"
const NETSEARCH_CONTRACTID = "@mozilla.org/rdf/datasource;1?name=internetsearch"
const nsISupports      = Components.interfaces.nsISupports;
const nsIFactory       = Components.interfaces.nsIFactory;
const nsISidebar       = Components.interfaces.nsISidebar;
const nsIRDFContainer  = Components.interfaces.nsIRDFContainer;
const nsIProperties    = Components.interfaces.nsIProperties;
const nsIFileURL       = Components.interfaces.nsIFileURL;
const nsIRDFRemoteDataSource = Components.interfaces.nsIRDFRemoteDataSource;
const nsIInternetSearchService = Components.interfaces.nsIInternetSearchService;
const nsIClassInfo = Components.interfaces.nsIClassInfo;

function nsSidebar()
{
    const RDF_CONTRACTID = "@mozilla.org/rdf/rdf-service;1";
    const nsIRDFService = Components.interfaces.nsIRDFService;
    
    this.rdf = Components.classes[RDF_CONTRACTID].getService(nsIRDFService);
    this.datasource_uri = getSidebarDatasourceURI(PANELS_RDF_FILE);
    debug('datasource_uri is ' + this.datasource_uri);
    this.resource = 'urn:sidebar:current-panel-list';
    this.datasource = this.rdf.GetDataSource(this.datasource_uri);
}

nsSidebar.prototype.nc = "http://home.netscape.com/NC-rdf#";

nsSidebar.prototype.setWindow =
function (aWindow)
{    
    this.window = aWindow;    
}

nsSidebar.prototype.isPanel =
function (aContentURL)
{
    var container = 
        Components.classes[CONTAINER_CONTRACTID].createInstance(nsIRDFContainer);

    container.Init(this.datasource, this.rdf.GetResource(this.resource));
    
    /* Create a resource for the new panel and add it to the list */
    var panel_resource = 
        this.rdf.GetResource("urn:sidebar:3rdparty-panel:" + aContentURL);

    return (container.IndexOf(panel_resource) != -1);
}

function sidebarURLSecurityCheck(url)
{
    if (url.search(/(^http:|^ftp:|^https:)/) == -1)
        throw "Script attempted to add sidebar panel from illegal source";
}

/* decorate prototype to provide ``class'' methods and property accessors */
nsSidebar.prototype.addPanel =
function (aTitle, aContentURL, aCustomizeURL)
{
    debug("addPanel(" + aTitle + ", " + aContentURL + ", " +
          aCustomizeURL + ")");

    if (!this.window)
    {
        debug ("no window object set, bailing out.");
        throw Components.results.NS_ERROR_NOT_INITIALIZED;
    }

    sidebarURLSecurityCheck(aContentURL);

    // Create a "container" wrapper around the current panels to
    // manipulate the RDF:Seq more easily.
    var panel_list = this.datasource.GetTarget(this.rdf.GetResource(this.resource), this.rdf.GetResource(nsSidebar.prototype.nc+"panel-list"), true);
    if (panel_list) {
        panel_list.QueryInterface(Components.interfaces.nsIRDFResource);
    } else {
        // Datasource is busted. Start over.
        debug("Sidebar datasource is busted\n");
  }

    var container = Components.classes[CONTAINER_CONTRACTID].createInstance(nsIRDFContainer);
    container.Init(this.datasource, panel_list);

    /* Create a resource for the new panel and add it to the list */
    var panel_resource = 
        this.rdf.GetResource("urn:sidebar:3rdparty-panel:" + aContentURL);
    var panel_index = container.IndexOf(panel_resource);
    var stringBundle, brandStringBundle, titleMessage, dialogMessage, promptService ;
    if (panel_index != -1)
    {
        try {
            stringBundle = srGetStrBundle("chrome://communicator/locale/sidebar/sidebar.properties");
            brandStringBundle = srGetStrBundle("chrome://global/locale/brand.properties");
            if (stringBundle) {
                sidebarName = brandStringBundle.GetStringFromName("sidebarName");
                titleMessage = stringBundle.GetStringFromName("dupePanelAlertTitle");
                dialogMessage = stringBundle.GetStringFromName("dupePanelAlertMessage");
                dialogMessage = dialogMessage.replace(/%url%/, aContentURL);
                dialogMessage = dialogMessage.replace(/%name%/, sidebarName);
            }
        }
        catch (e) {
            titleMessage = "Sidebar";
            dialogMessage = aContentURL + " already exists in Sidebar.  No string bundle";
        }
          
        promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
        if (promptService) {
          promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);
          promptService.alert(this.window, titleMessage, dialogMessage);
        }

        return;
    }

    try {
        stringBundle = srGetStrBundle("chrome://communicator/locale/sidebar/sidebar.properties");
        brandStringBundle = srGetStrBundle("chrome://global/locale/brand.properties");
        if (stringBundle) {
            sidebarName = brandStringBundle.GetStringFromName("sidebarName");
            titleMessage = stringBundle.GetStringFromName("addPanelConfirmTitle");
            dialogMessage = stringBundle.GetStringFromName("addPanelConfirmMessage");
            dialogMessage = dialogMessage.replace(/%title%/, aTitle);
            dialogMessage = dialogMessage.replace(/%url%/, aContentURL);
            dialogMessage = dialogMessage.replace(/#/g, "\n");
            dialogMessage = dialogMessage.replace(/%name%/, sidebarName);
        }
    }
    catch (e) {
        titleMessage = "Add Tab to Sidebar";
        dialogMessage = "No string bundle.  Add the Tab '" + aTitle + "' to Sidebar?\n\n" + "Source: " + aContentURL;
    }
          
    promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
    promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);
    var rv = promptService.confirm(this.window, titleMessage, dialogMessage);
      
    if (!rv)
        return;

    /* Now make some sidebar-ish assertions about it... */
    this.datasource.Assert(panel_resource,
                           this.rdf.GetResource(this.nc + "title"),
                           this.rdf.GetLiteral(aTitle),
                           true);
    this.datasource.Assert(panel_resource,
                           this.rdf.GetResource(this.nc + "content"),
                           this.rdf.GetLiteral(aContentURL),
                           true);
    if (aCustomizeURL)
        this.datasource.Assert(panel_resource,
                               this.rdf.GetResource(this.nc + "customize"),
                               this.rdf.GetLiteral(aCustomizeURL),
                               true);
        
    container.AppendElement(panel_resource);

    // Use an assertion to pass a "refresh" event to all the sidebars.
    // They use observers to watch for this assertion (in sidebarOverlay.js).
    this.datasource.Assert(this.rdf.GetResource(this.resource),
                           this.rdf.GetResource(this.nc + "refresh"),
                           this.rdf.GetLiteral("true"),
                           true);
    this.datasource.Unassert(this.rdf.GetResource(this.resource),
                             this.rdf.GetResource(this.nc + "refresh"),
                             this.rdf.GetLiteral("true"));

    /* Write the modified panels out. */
    this.datasource.QueryInterface(nsIRDFRemoteDataSource).Flush();

}

/* decorate prototype to provide ``class'' methods and property accessors */
nsSidebar.prototype.addSearchEngine =
function (engineURL, iconURL, suggestedTitle, suggestedCategory)
{
    debug("addSearchEngine(" + engineURL + ", " + iconURL + ", " +
          suggestedCategory + ", " + suggestedTitle + ")");

    if (!this.window)
    {
        debug ("no window object set, bailing out.");
        throw Components.results.NS_ERROR_NOT_INITIALIZED;
    }

    try
    {
        // make sure using HTTP (for both engine as well as icon URLs)

        if (engineURL.search(/^http:\/\//i) == -1)
        {
            debug ("must use HTTP to fetch search engine file");
            throw Components.results.NS_ERROR_INVALID_ARG;
        }

        if (iconURL.search(/^http:\/\//i) == -1)
        {
            debug ("must use HTTP to fetch search icon file");
            throw Components.results.NS_ERROR_INVALID_ARG;
        }

        // make sure engineURL refers to a .src file
        if (engineURL.search(/\.src$/i) == -1)
        {
            debug ("engineURL doesn't reference a .src file");
            throw Components.results.NS_ERROR_INVALID_ARG;
        }

        // make sure iconURL refers to a .gif/.jpg/.jpeg/.png file
        if (iconURL.search(/\.(gif|jpg|jpeg|png)$/i) == -1)
        {
            debug ("iconURL doesn't reference a supported image file");
            throw Components.results.NS_ERROR_INVALID_ARG;
        }

    }
    catch(ex)
    {
        this.window.alert("Failed to add the search engine.\n");
        throw Components.results.NS_ERROR_INVALID_ARG;
    }

    var titleMessage, dialogMessage;
    try {
        var stringBundle = srGetStrBundle("chrome://communicator/locale/sidebar/sidebar.properties");
        var brandStringBundle = srGetStrBundle("chrome://global/locale/brand.properties");
        if (stringBundle) {
            sidebarName = brandStringBundle.GetStringFromName("sidebarName");            
            titleMessage = stringBundle.GetStringFromName("addEngineConfirmTitle");
            dialogMessage = stringBundle.GetStringFromName("addEngineConfirmMessage");
            dialogMessage = dialogMessage.replace(/%title%/, suggestedTitle);
            dialogMessage = dialogMessage.replace(/%category%/, suggestedCategory);
            dialogMessage = dialogMessage.replace(/%url%/, engineURL);
            dialogMessage = dialogMessage.replace(/#/g, "\n");
            dialogMessage = dialogMessage.replace(/%name%/, sidebarName);
        }
    }
    catch (e) {
        titleMessage = "Add Search Engine";
        dialogMessage = "Add the following search engine?\n\nName: " + suggestedTitle;
        dialogMessage += "\nSearch Category: " + suggestedCategory;
        dialogMessage += "\nSource: " + engineURL;
    }
          
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
    promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);
    var rv = promptService.confirm(this.window, titleMessage, dialogMessage);
      
    if (!rv)
        return;

    var internetSearch = Components.classes[NETSEARCH_CONTRACTID].getService();
    if (internetSearch)    
        internetSearch = internetSearch.QueryInterface(nsIInternetSearchService);
    if (internetSearch)
    {
        internetSearch.AddSearchEngine(engineURL, iconURL, suggestedTitle,
                                       suggestedCategory);
    }
}

// property of nsIClassInfo
nsSidebar.prototype.flags = nsIClassInfo.DOM_OBJECT;

// property of nsIClassInfo
nsSidebar.prototype.classDescription = "Sidebar";

nsSidebar.prototype.QueryInterface =
function (iid) {
    if (!iid.equals(nsISidebar) && 
        !iid.equals(nsIClassInfo) &&
        !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
}

var sidebarModule = new Object();

sidebarModule.registerSelf =
function (compMgr, fileSpec, location, type)
{
    debug("registering (all right -- a JavaScript module!)");
    compMgr.registerComponentWithType(SIDEBAR_CID, "Sidebar JS Component",
                                      SIDEBAR_CONTRACTID, fileSpec, location,
                                      true, true, type);
}

sidebarModule.getClassObject =
function (compMgr, cid, iid) {
    if (!cid.equals(SIDEBAR_CID))
        throw Components.results.NS_ERROR_NO_INTERFACE;
    
    if (!iid.equals(Components.interfaces.nsIFactory))
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    
    return sidebarFactory;
}

sidebarModule.canUnload =
function(compMgr)
{
    debug("Unloading component.");
    return true;
}
    
/* factory object */
var sidebarFactory = new Object();

sidebarFactory.createInstance =
function (outer, iid) {
    debug("CI: " + iid);
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    return (new nsSidebar()).QueryInterface(iid);
}

/* entrypoint */
function NSGetModule(compMgr, fileSpec) {
    return sidebarModule;
}

/* static functions */
if (DEBUG)
    debug = function (s) { dump("-*- sidebar component: " + s + "\n"); }
else
    debug = function (s) {}

function getSidebarDatasourceURI(panels_file_id)
{
    try 
    {
        /* use the fileLocator to look in the profile directory 
         * to find 'panels.rdf', which is the
         * database of the user's currently selected panels. */
        var directory_service = Components.classes[DIR_SERV_CONTRACTID].getService();
        if (directory_service)
            directory_service = directory_service.QueryInterface(Components.interfaces.nsIProperties);

        /* if <profile>/panels.rdf doesn't exist, get will copy
         *bin/defaults/profile/panels.rdf to <profile>/panels.rdf */
        var sidebar_file = directory_service.get(panels_file_id, Components.interfaces.nsIFile);

        if (!sidebar_file.exists())
        {
            /* this should not happen, as GetFileLocation() should copy
             * defaults/panels.rdf to the users profile directory */
            debug("sidebar file does not exist");
            return null;
        }

        debug("sidebar uri is " + sidebar_file.URL);
        return sidebar_file.URL;
    }
    catch (ex)
    {
        /* this should not happen */
        debug("caught " + ex + " getting sidebar datasource uri");
        return null;
    }
}


var strBundleService = null;
function srGetStrBundle(path)
{
   var strBundle = null;
   if (!strBundleService) {
       try {
          strBundleService =
          Components.classes["@mozilla.org/intl/stringbundle;1"].getService(); 
          strBundleService = 
          strBundleService.QueryInterface(Components.interfaces.nsIStringBundleService);
       } catch (ex) {
          dump("\n--** strBundleService failed: " + ex + "\n");
          return null;
      }
   }
   strBundle = strBundleService.createBundle(path); 
   if (!strBundle) {
       dump("\n--** strBundle createInstance failed **--\n");
   }
   return strBundle;
}


