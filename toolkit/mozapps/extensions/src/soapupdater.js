// XXXben - this is not used by anything, it's just a backup of some code I did have in nsExtensionManager.js.in

function nsExtensionUpdater2(aExtensions,
                             aTargetAppID, aTargetAppVersion)
{
  this._extensions = aExtensions;
  this._count = aExtensions.length;
  this._appID = aTargetAppID;
  this._appVersion = aTargetAppVersion;

  this._os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
}

nsExtensionUpdater2.prototype = {
  _extensionSchemaType: null,
  loadSchema: function ()
  {
    function SchemaLoaderListener(aUpdater)
    {
      this._updater = aUpdater;
    }
    
    SchemaLoaderListener.prototype = {
      onLoad: function (aSchema)
      {
        dump("*** schema loaded = " + aSchema + "\n");
        var count = aSchema.typeCount;
        for (var i = 0; i < count; ++i) {
          var type = aSchema.getTypeByIndex(i);
          dump("*** schema type = " + type.name + "\n");
        }
        
        this._updater._extensionSchemaType = aSchema.getTypeByName("Extension");
        
        this._updater._schemaLoaded();
      },

      onError: function (aStatus, aMessage)
      {
        dump("*** schema load error " + aStatus + ", msg = " + aMessage + "\n");
      }
    };
  
    var schemaLoader = Components.classes["@mozilla.org/xmlextras/schemas/schemaloader;1"]
                                 .createInstance(Components.interfaces.nsISchemaLoader);
    var schemaLoaderListener = new SchemaLoaderListener(this);
    schemaLoader.loadAsync("http://www.bengoodger.com/software/mb/umo/types.xsd", 
                           schemaLoaderListener);
  },
  
  _schemaLoaded: function ()
  {
    var call = Components.classes["@mozilla.org/xmlextras/soap/call;1"]
                         .createInstance(Components.interfaces.nsISOAPCall);
    call.transportURI = "http://localhost:8080/axis/services/VersionCheck";
    
    for (var i = 0; i < this._extensions.length; ++i) {
      var e = this._extensions[i];
      
      var params = [this._createParam("in0", e, this._extensionSchemaType),
                    this._createParam("in1", this._appID, null),
                    this._createParam("in2", this._appVersion, null)];
      call.encode(0, "getNewestExtension", "urn:VersionCheck", 0, null, params.length, params);
      var response = call.invoke();
      dump("*** response = " + response + "\n");
      var count = { };
      var params;
      response.getParameters(false, count, params);
      dump("*** params = " + count.value + "\n");
      for (var j = 0; j < count.value; ++j) {
        var param = params[j];
        dump("*** param = " + param.name + ", parm = " + param + ", element = " + param.element + ", value = " + param.value + "\n");
        // var v = param.value.QueryInterface(Components.interfaces.nsIVariant);
      }
    }
  },
  
  _walkKids: function (e)
  {
    for (var i = 0; i < e.childNodes.length; ++i) {
      var kid = e.childNodes[i];
      dump("<" + kid.nodeName);
      for (var k = 0; k < kid.attributes.length; ++k)
        dump(" " + kid.attributes[k] + "=" + kid.getAttribute(kid.attributes[k]));
      if (kid.hasChildNodes()) {
        this._walkKids(kid);
        dump("</" + kid.nodeName);
      }
      else
        dump(">");
    }
  },
  
  _createParam: function (aParamName, aParamValue, aParamSchemaType)
  {
    var param = Components.classes["@mozilla.org/xmlextras/soap/parameter;1"]
                          .createInstance(Components.interfaces.nsISOAPParameter);
    param.name = aParamName;
    param.namespaceURI = "urn:VersionCheck";
    if (aParamSchemaType)
      param.schemaType = aParamSchemaType;
    param.value = aParamValue;
    return param;
  },

};


// this will come back later when we do custom update urls
  getUpdateURLs: function (aExtensionID)
  {
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    var appID = pref.getCharPref(PREF_EM_APP_ID);
  
    var urls = [];
    if (aExtensionID) {
      var updateURL = this._getUpdateURLInternal(aExtensionID);
      updateURL = updateURL.replace(/%APP%/g, escape(appID));
      updateURL = updateURL.replace(/%ITEM%/g, escape(aExtensionID));
      urls.push(updateURL);
    }
    else {
      var ctr = Components.classes["@mozilla.org/rdf/container;1"]
                          .createInstance(Components.interfaces.nsIRDFContainer);
      ctr.Init(this, this._rdf.GetResource("urn:mozilla:extension:root"));
      
      var urlHash = { };
      
      var e = ctr.GetElements();
      while (e.hasMoreElements()) {
        var r = e.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
        var extensionID = r.Value.substr("urn:mozilla:extension:".length, r.Value.length);
        var updateURL = this._getUpdateURLInternal(extensionID);
        if (!(updateURL in urlHash))
          urlHash[updateURL] = [];
          
        urlHash[updateURL].push(extensionID);
      }
      
      for (var url in urlHash) {
        var guidString = "";
        var urlCount = urlHash[url].length;
        for (var i = 0; i < urlCount; ++i)
          guidString += escape(urlHash[url][i] + (i < urlCount - 1 ? "," : ""));
        url = url.replace(/%APP%/g, appID);
        url = url.replace(/%ITEM%/g, guidString);
        urls.push(url);
      }
    }
    return urls;
  },
  
  _getUpdateURLInternal: function (aExtensionID)
  {
    var updateURL;
    var extension = this._rdf.GetResource("urn:mozilla:extension:" + aExtensionID);
   
    if (this.hasArcOut(extension, this._emR("updateURL"))) {
      updateURL = this.GetTarget(extension, this._emR("updateURL"), true);
      if (updateURL) 
        updateURL = updateURL.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
    }
    
    if (!updateURL) {
      var pref = Components.classes["@mozilla.org/preferences-service;1"]
                           .getService(Components.interfaces.nsIPrefBranch);
      updateURL = pref.getCharPref(PREF_EM_DEFAULTUPDATEURL);
    }
    return updateURL;
  },
  


///////////////////////////////////////////////////////////////////////////////
//
// nsJarFileExtractor
//
function nsJarFileExtractor(aXPIFile, aTargetDir)
{
  this._xpiFile = aXPIFile.path;
  this._targetDir = aTargetDir.path;
  // this._proxyObject(Components, Components.interfaces.nsIXPCComponents, "_components");
  /* 
  this._proxyObject(aXPIFile,   Components.interfaces.nsIFile, "_xpiFile");
  this._proxyObject(aTargetDir, Components.interfaces.nsIFile, "_targetDir");
   */
}

nsJarFileExtractor.prototype = {
  // proxied objects
  _xpiFile: null,
  _targetDir: null,
  _components: null,
  
  _proxyObject: function (aObject, aIID, aTarget)
  {
    const nsIEventQueueService = Components.interfaces.nsIEventQueueService;
    var eqService = Components.classes["@mozilla.org/event-queue-service;1"]
                              .getService(nsIEventQueueService);
    var uiQ = eqService.getSpecialEventQueue(nsIEventQueueService.UI_THREAD_EVENT_QUEUE);
    
    var proxyObjectManager = Components.classes["@mozilla.org/xpcomproxy;1"]
                                       .getService(Components.interfaces.nsIProxyObjectManager);
    const PROXY_SYNC = 0x01;
    const PROXY_ALWAYS = 0x04;
    this[aTarget] = proxyObjectManager.getProxyForObject(uiQ, aIID, aObject, 
                                                         PROXY_SYNC | PROXY_ALWAYS);
  },
  
  extract: function ()
  {
    const nsIThread = Components.interfaces.nsIThread;
    var thread = Components.classes["@mozilla.org/thread;1"]
                           .createInstance(nsIThread);
    thread.init(this, 0, nsIThread.PRIORITY_NORMAL,
                nsIThread.SCOPE_GLOBAL,
                nsIThread.STATE_JOINABLE);
  },

  /////////////////////////////////////////////////////////////////////////////
  // nsIRunnable
  run: function ()
  { 
    dump("*** RUNNING THREAD\n");
    /*
    var xpiFile = Components.classes["@mozilla.org/file/local;1"]
                            .createInstance(Components.interfaces.nsILocalFile);
    xpiFile.initWithPath(this._xpiFile);
    var targetDir = Components.classes["@mozilla.org/file/local;1"]
                              .createInstance(Components.interfaces.nsILocalFile);
    targetDir.initWithPath(this._targetDir);
    
    var zipReader = Components.classes["@mozilla.org/libjar/zip-reader;1"]
                              .createInstance(Components.interfaces.nsIZipReader);
    zipReader.init(xpiFile);

    var entries = zipReader.findEntries("*");
    while (entries.hasMoreElements()) {
      var entry = entries.getNext().QueryInterface(Components.interfaces.nsIZipEntry);
      dump("*** zip entry = " + entry.name + "\n");
    }
    */
  }
};

