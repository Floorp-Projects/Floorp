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
  
