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
 * The Original Code is Plugin Finder Service.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the IBM Corporation are Copyright (C) 2004-2005
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Doron Rosenberg <doronr@us.ibm.com>
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

const RDF_NS = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const PFS_NS = "http://www.mozilla.org/2004/pfs-rdf#";

function nsRDFItemUpdater(aClientOS, aChromeLocale) {
  this._rdfService = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                               .getService(Components.interfaces.nsIRDFService);
  this._os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);

  var app = Components.classes["@mozilla.org/xre/app-info;1"]
                      .getService(Components.interfaces.nsIXULAppInfo);
  this.appID = app.ID;
  this.buildID = app.platformBuildID;
  this.appRelease = app.version;

  this.clientOS = aClientOS;
  this.chromeLocale = aChromeLocale;

  var prefBranch = Components.classes["@mozilla.org/preferences-service;1"]
                             .getService(Components.interfaces.nsIPrefBranch);
  this.dsURI = prefBranch.getCharPref("pfs.datasource.url");
}

nsRDFItemUpdater.prototype = {
  checkForPlugin: function (aPluginRequestItem) {
    var dsURI = this.dsURI;
    // escape the mimetype as mimetypes can contain '+', which will break pfs.
    dsURI = dsURI.replace(/%PLUGIN_MIMETYPE%/g, encodeURIComponent(aPluginRequestItem.mimetype));
    dsURI = dsURI.replace(/%APP_ID%/g, this.appID);
    dsURI = dsURI.replace(/%APP_VERSION%/g, this.buildID);
    dsURI = dsURI.replace(/%APP_RELEASE%/g, this.appRelease);
    dsURI = dsURI.replace(/%CLIENT_OS%/g, this.clientOS);
    dsURI = dsURI.replace(/%CHROME_LOCALE%/g, this.chromeLocale);

    var ds = this._rdfService.GetDataSource(dsURI);
    var rds = ds.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource)
    if (rds.loaded)
      this.onDatasourceLoaded(ds, aPluginRequestItem);
    else {
      var sink = ds.QueryInterface(Components.interfaces.nsIRDFXMLSink);
      sink.addXMLSinkObserver(new nsPluginXMLRDFDSObserver(this, aPluginRequestItem));
    }
  },

  onDatasourceLoaded: function pfs_onDatasourceLoaded (aDatasource, aPluginRequestItem) {
    var container = Components.classes["@mozilla.org/rdf/container;1"]
                              .createInstance(Components.interfaces.nsIRDFContainer);
    var resultRes = this._rdfService.GetResource("urn:mozilla:plugin-results:" + aPluginRequestItem.mimetype);
    var pluginList = aDatasource.GetTarget(resultRes, this._rdfService.GetResource(PFS_NS+"plugins"), true);

    var pluginInfo = null;
  
    try {
      container.Init(aDatasource, pluginList);

      var children = container.GetElements();
      var target;

      // get the first item
      var child = children.getNext();
      if (child instanceof Components.interfaces.nsIRDFResource) {
        var name = this._rdfService.GetResource("http://www.mozilla.org/2004/pfs-rdf#updates");
        target = aDatasource.GetTarget(child, name, true);
      }

      try {
        container.Init(aDatasource, target);
        target = null;
        children = container.GetElements();

        var child = children.getNext();
        if (child instanceof Components.interfaces.nsIRDFResource) {
          target = child;
        }

        var rdfs = this._rdfService;

        function getPFSValueFromRDF(aValue) {
          var rv = null;

          var myTarget = aDatasource.GetTarget(target, rdfs.GetResource(PFS_NS + aValue), true);
          if (myTarget)
            rv = myTarget.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;

          return rv;
        }

        pluginInfo = {
          name: getPFSValueFromRDF("name"),
          pid: getPFSValueFromRDF("guid"),
          version: getPFSValueFromRDF("version"),
          IconUrl: getPFSValueFromRDF("IconUrl"),
          InstallerLocation: getPFSValueFromRDF("InstallerLocation"),
          InstallerHash: getPFSValueFromRDF("InstallerHash"),
          XPILocation: getPFSValueFromRDF("XPILocation"),
          XPIHash: getPFSValueFromRDF("XPIHash"),
          InstallerShowsUI: getPFSValueFromRDF("InstallerShowsUI"),
          manualInstallationURL: getPFSValueFromRDF("manualInstallationURL"),
          requestedMimetype: getPFSValueFromRDF("requestedMimetype"),
          licenseURL: getPFSValueFromRDF("licenseURL"),
          needsRestart: getPFSValueFromRDF("needsRestart")
        };
      }
      catch (ex) {
        Components.utils.reportError(ex);
      }
    }
    catch (ex) {
      Components.utils.reportError(ex);
    }
    
    gPluginInstaller.pluginInfoReceived(aPluginRequestItem, pluginInfo);
  },

  onDatasourceError: function pfs_onDatasourceError (aPluginRequestItem, aError) {
    this._os.notifyObservers(aPluginRequestItem, "error", aError);
    Components.utils.reportError(aError);
    gPluginInstaller.pluginInfoReceived(aPluginRequestItem, null);
  }
};

function nsPluginXMLRDFDSObserver(aUpdater, aPluginRequestItem) {
  this._updater = aUpdater;
  this._item    = aPluginRequestItem;
}

nsPluginXMLRDFDSObserver.prototype = 
{ 
  _updater  : null,
  _item     : null,

  // nsIRDFXMLSinkObserver
  onBeginLoad: function(aSink) {},
  onInterrupt: function(aSink) {},
  onResume: function(aSink) {},
  onEndLoad: function(aSink) {
    aSink.removeXMLSinkObserver(this);
    
    var ds = aSink.QueryInterface(Components.interfaces.nsIRDFDataSource);
    this._updater.onDatasourceLoaded(ds, this._item);
  },
  
  onError: function(aSink, aStatus, aErrorMsg) {  
    aSink.removeXMLSinkObserver(this);   
    this._updater.onDatasourceError(this._item, aStatus.toString());
  }
};
