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
 * Contributor(s): Stephen Lamm <slamm@netscape.com>
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
function mySidebar() { /* big comment for no code, eh? */ }

/* decorate prototype to provide ``class'' methods and property accessors */
mySidebar.prototype = {
    open: function () {
        debug("mySidebar::open()");
        if (!document) return;
        splitter = top.document.getElementById('sidebar-splitter')
        if (!splitter) return;
        splitter.removeAttribute('collapsed');
    },
    addPanel: function (aTitle, aContentURL, aCustomizeURL) {
        debug("mySidebar::addPanel("+aTitle+", "+aContentURL+", "
             + aCustomizeURL+")");

        var appShell = Components.classes['component://netscape/appshell/appShellService'].getService();
        appShell = appShell.QueryInterface(Components.interfaces.nsIAppShellService);

        // Create a "container" wrapper around the
        // "urn:sidebar:ccurent-panel-list" object. This makes it easier
        // to manipulate the RDF:Seq correctly.
        var container = Components.classes["component://netscape/rdf/container"].createInstance();
        container = container.QueryInterface(Components.interfaces.nsIRDFContainer);
        // Create a resource for the new panel and add it to the list
        var panel_resource = this.rdf.GetResource("urn:sidebar:3rdparty-panel:"+aContentURL);
        var panel_index = container.IndexOf(panel_resource);
        if (panel_index != -1) {
          appShell.RunModalDialog(null,null,
                                  "chrome://sidebar/content/panel-exists.xul")
          return;
        }
        // Throw up modal dialog asking for permission to add panel.
        // Check for "Add Panel", "Cancel" response.
        hidden_window.openDialog("chrome://sidebar/content/panel-add-confirm.xul",
                                 "_blank", "chrome,close,titlebar,modal", "");

        debug("  this.datasource ="+this.datasource);
        debug("  this.resource ="+this.resource);
        container.Init(this.datasource, this.rdf.GetResource(this.resource));

        container.AppendElement(panel_resource);

        // Now make some sidebar-ish assertions about it...
        this.datasource.Assert(panel_resource,
                               this.rdf.GetResource(this.nc + "title"),
                               this.rdf.GetLiteral(aTitle),
                               true);
        this.datasource.Assert(panel_resource,
                               this.rdf.GetResource(this.nc + "content"),
                               this.rdf.GetLiteral(aContentURL),
                               true);
        if (aCustomizeURL && aCustomizeURL != "") {
            this.datasource.Assert(panel_resource,
                                   this.rdf.GetResource(this.nc + "customize"),
                                   this.rdf.GetLiteral(aCustomizeURL),
                                   true);
        }
        
        // Write the modified panels out.
        this.datasource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource).Flush();

    },
    setPanelTitle: function (aTitle) {
        debug("mySidebar::setPanelTitle("+aTitle+")");
    },
    init: function() {
        // the magic number to find panels.rdf
        var PANELS_RDF_FILE = 66626;

        // The rdf service
        this.rdf = 'component://netscape/rdf/rdf-service'
        this.rdf = Components.classes[this.rdf].getService();
        this.rdf = this.rdf.QueryInterface(Components.interfaces.nsIRDFService);
        this.nc = "http://home.netscape.com/NC-rdf#";
        this.datasource_uri = getSidebarDatasourceURI(PANELS_RDF_FILE);
        debug('Sidebar datasource_uri is '+this.datasource_uri);
        this.resource = 'urn:sidebar:current-panel-list';
        this.datasource = this.rdf.GetDataSource(this.datasource_uri);
    }
}

var myModule = {
    firstTime: true,

    /*
     * RegisterSelf is called at registration time (component installation
     * or the only-until-release startup autoregistration) and is responsible
     * for notifying the component manager of all components implemented in
     * this module.  The fileSpec, location and type parameters are mostly
     * opaque, and should be passed on to the registerComponent call
     * unmolested.
     */
    registerSelf: function (compMgr, fileSpec, location, type) {
        if (0 && this.firstTime) {
            debug("*s** Deferring registration of sidebar JS components");
            this.firstTime = false;
            throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
        }
        debug("*** Registering sidebar JS components");
        compMgr.registerComponentWithType(this.myCID,
                                          "Sidebar JS Component",
                                          "component://mozilla/sidebar.1", fileSpec,
                                          location, true, true, 
                                          type);
    },

    /*
     * The GetClassObject method is responsible for producing Factory and
     * SingletonFactory objects (the latter are specialized for services).
     */
    getClassObject: function (compMgr, cid, iid) {
        if (!cid.equals(this.myCID))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        return this.myFactory;
    },

    /* CID for this class */
    myCID: Components.ID("{22117140-9c6e-11d3-aaf1-00805f8a4905}"),

    /* factory object */
    myFactory: {
        /*
         * Construct an instance of the interface specified by iid,
         * possibly aggregating with the provided |outer|.  (If you don't
         * know what aggregation is all about, you don't need to.  It reduces
         * even the mightiest of XPCOM warriors to snivelling cowards.)
         */
        CreateInstance: function (outer, iid) {
            debug("CI: " + iid);
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;

            /*
             * If we had a QueryInterface method (see above), we would write
             * the following as:
             *    return (new mySidebar()).QueryInterface(iid);
             * because our QI would check the IID correctly for us.
             */
            
            if (!iid.equals(Components.interfaces.nsISidebar) &&
                !iid.equals(Components.interfaces.nsISupports)) {
                throw Components.results.NS_ERROR_INVALID_ARG;
            }

            return new mySidebar();
        }
    },

    /*
     * canUnload is used to signal that the component is about to be unloaded.
     * C++ components can return false to indicate that they don't wish to
     * be unloaded, but the return value from JS components' canUnload is
     * ignored: mark-and-sweep will keep everything around until it's no
     * longer in use, making unconditional ``unload'' safe.
     *
     * You still need to provide a (likely useless) canUnload method, though:
     * it's part of the nsIModule interface contract, and the JS loader _will_
     * call it.
     */
    canUnload: function(compMgr) {
        debug("*** Unloading sidebar JS components.");
        return true;
    }
};
    
function NSGetModule(compMgr, fileSpec) {
    return myModule;
}

function debug(s)
{
   dump(s+"\n");
}

function getSidebarDatasourceURI(panels_file_id) {
  try {
	  var fileLocatorInterface = Components.interfaces.nsIFileLocator;
	  var fileLocatorProgID = 'component://netscape/filelocator';
	  var fileLocatorService  = Components.classes[fileLocatorProgID].getService();
	  // use the fileLocator to look in the profile directory 
          // to find 'panels.rdf', which is the
	  // database of the user's currently selected panels.
	  fileLocatorService = fileLocatorService.QueryInterface(fileLocatorInterface);

	  // if <profile>/panels.rdf doesn't exist, GetFileLocation() will copy
	  // bin/defaults/profile/panels.rdf to <profile>/panels.rdf
	  var sidebar_file = fileLocatorService.GetFileLocation(panels_file_id);

	  if (!sidebar_file.exists()) {	
		// this should not happen, as GetFileLocation() should copy
		// defaults/panels.rdf to the users profile directory
        debug("sidebar file does not exist");
		return null;
	  }

	  debug("sidebar uri is " + sidebar_file.URLString);
	  return sidebar_file.URLString;
  }
  catch (ex) {
	// this should not happen
    debug("Exception raise getting sidebar datasource uri");
	return null;
  }
}
