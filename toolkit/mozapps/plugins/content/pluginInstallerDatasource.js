const RDF_NS = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const PFS_NS = "http://www.mozilla.org/2004/pfs-rdf#";

function nsRDFItemUpdater(aClientOS, aChromeLocale){
  this._rdfService = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                        .getService(Components.interfaces.nsIRDFService);
  this._os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);

  var prefBranch = Components.classes["@mozilla.org/preferences-service;1"]
                             .getService(Components.interfaces.nsIPrefBranch);
  this.appID = prefBranch.getCharPref("app.id");
  this.buildID = prefBranch.getCharPref("app.build_id");
  this.clientOS = aClientOS;
  this.chromeLocale = aChromeLocale;
  this.dsURI = prefBranch.getComplexValue("pfs.datasource.url",
                                     Components.interfaces.nsIPrefLocalizedString).data;
}

nsRDFItemUpdater.prototype = {
  checkForPlugin: function (aPluginRequestItem){
    var dsURI = this.dsURI;
    dsURI = dsURI.replace(/%PLUGIN_MIMETYPE%/g, aPluginRequestItem.mimetype);
    dsURI = dsURI.replace(/%APP_ID%/g, this.appID);
    dsURI = dsURI.replace(/%APP_VERSION%/g, this.buildID);
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

  onDatasourceLoaded: function pfs_onDatasourceLoaded (aDatasource, aPluginRequestItem){
    var container = Components.classes["@mozilla.org/rdf/container;1"].
                  createInstance(Components.interfaces.nsIRDFContainer);
    var resultRes = this._rdfService.GetResource("urn:mozilla:plugin-results:" + aPluginRequestItem.mimetype);
    var pluginList = aDatasource.GetTarget(resultRes, this._rdfService.GetResource(PFS_NS+"plugins"), true);

    var pluginInfo = null;
  
    container = Components.classes["@mozilla.org/rdf/container;1"].createInstance(Components.interfaces.nsIRDFContainer);
    try {
      container.Init(aDatasource, pluginList);

      var children = container.GetElements();
      var target;

      // get the first item
      var child = children.getNext();
      if (child instanceof Components.interfaces.nsIRDFResource){
        var name = this._rdfService.GetResource("http://www.mozilla.org/2004/pfs-rdf#updates");
        target = aDatasource.GetTarget(child, name, true);
      }

      try {
        container.Init(aDatasource, target);
        target = null;
        children = container.GetElements();

        var child = children.getNext();
        if (child instanceof Components.interfaces.nsIRDFResource){
          target = child;
        }

        function getPFSValueFromRDF(aValue, aDatasource, aRDFService){
          var rv = null;

          var myTarget = aDatasource.GetTarget(target, aRDFService.GetResource(PFS_NS + aValue), true);
          if (myTarget)
            rv = myTarget.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;

          return rv;
        }

        pluginInfo = new Object();
        pluginInfo.name = getPFSValueFromRDF("name", aDatasource, this._rdfService);
        pluginInfo.pid = getPFSValueFromRDF("guid", aDatasource, this._rdfService);
        pluginInfo.version = getPFSValueFromRDF("version", aDatasource, this._rdfService);
        pluginInfo.IconUrl = getPFSValueFromRDF("IconUrl", aDatasource, this._rdfService);
        pluginInfo.XPILocation = getPFSValueFromRDF("XPILocation", aDatasource, this._rdfService);
        pluginInfo.InstallerShowsUI = getPFSValueFromRDF("InstallerShowsUI", aDatasource, this._rdfService);
        pluginInfo.manualInstallationURL = getPFSValueFromRDF("manualInstallationURL", aDatasource, this._rdfService);
        pluginInfo.requestedMimetype = getPFSValueFromRDF("requestedMimetype", aDatasource, this._rdfService);
        pluginInfo.licenseURL = getPFSValueFromRDF("licenseURL", aDatasource, this._rdfService);
        pluginInfo.needsRestart = getPFSValueFromRDF("needsRestart", aDatasource, this._rdfService);
      }
      catch (ex){}
    }
    catch (ex){}
    
    gPluginInstaller.pluginInfoReceived(pluginInfo);
  },

  onDatasourceError: function pfs_onDatasourceError (aPluginRequestItem, aError){
    this._os.notifyObservers(aPluginRequestItem, "error", aError);
    gPluginInstaller.pluginInfoReceived(null);
  },
};

function nsPluginXMLRDFDSObserver(aUpdater, aPluginRequestItem){
  this._updater = aUpdater;
  this._item    = aPluginRequestItem;
}

nsPluginXMLRDFDSObserver.prototype = 
{ 
  _updater  : null,
  _item     : null,

  // nsIRDFXMLSinkObserver
  onBeginLoad: function(aSink){},
  onInterrupt: function(aSink){},
  onResume: function(aSink){},
  onEndLoad: function(aSink){
    aSink.removeXMLSinkObserver(this);
    
    var ds = aSink.QueryInterface(Components.interfaces.nsIRDFDataSource);
    this._updater.onDatasourceLoaded(ds, this._item);
  },
  
  onError: function(aSink, aStatus, aErrorMsg){  
    aSink.removeXMLSinkObserver(this);   
    this._updater.onDatasourceError(this._item, aStatus.toString());
  }
};
