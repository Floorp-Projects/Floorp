/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;


const CLASS_MIMEINFO        = "mimetype";
const CLASS_PROTOCOLINFO    = "scheme";


// namespace prefix
const NC_NS                 = "http://home.netscape.com/NC-rdf#";

// the most recent default handlers that have been injected.  Note that
// this is used to construct an RDF resource, which needs to have NC_NS
// prepended, since that hasn't been done yet
const DEFAULT_HANDLERS_VERSION = "defaultHandlersVersion";

// type list properties

const NC_MIME_TYPES         = NC_NS + "MIME-types";
const NC_PROTOCOL_SCHEMES   = NC_NS + "Protocol-Schemes";

// content type ("type") properties

// nsIHandlerInfo::type
const NC_VALUE              = NC_NS + "value";

// additional extensions
const NC_FILE_EXTENSIONS    = NC_NS + "fileExtensions";

// references nsIHandlerInfo record
const NC_HANDLER_INFO       = NC_NS + "handlerProp";

// handler info ("info") properties

// nsIHandlerInfo::preferredAction
const NC_SAVE_TO_DISK       = NC_NS + "saveToDisk";
const NC_HANDLE_INTERNALLY  = NC_NS + "handleInternal";
const NC_USE_SYSTEM_DEFAULT = NC_NS + "useSystemDefault";

// nsIHandlerInfo::alwaysAskBeforeHandling
const NC_ALWAYS_ASK         = NC_NS + "alwaysAsk";

// references nsIHandlerApp records
const NC_PREFERRED_APP      = NC_NS + "externalApplication";
const NC_POSSIBLE_APP       = NC_NS + "possibleApplication";

// handler app ("handler") properties

// nsIHandlerApp::name
const NC_PRETTY_NAME        = NC_NS + "prettyName";

// nsILocalHandlerApp::executable
const NC_PATH               = NC_NS + "path";

// nsIWebHandlerApp::uriTemplate
const NC_URI_TEMPLATE       = NC_NS + "uriTemplate";

// nsIDBusHandlerApp::service
const NC_SERVICE            = NC_NS + "service";

// nsIDBusHandlerApp::method
const NC_METHOD             = NC_NS + "method";

// nsIDBusHandlerApp::objectPath
const NC_OBJPATH            = NC_NS + "objectPath";

// nsIDBusHandlerApp::dbusInterface
const NC_INTERFACE            = NC_NS + "dBusInterface";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");


function HandlerService() {
  this._init();
}

const HandlerServiceFactory = {
  _instance: null,
  createInstance: function (outer, iid) {
    if (this._instance)
      return this._instance;

    let processType = Cc["@mozilla.org/xre/runtime;1"].
      getService(Ci.nsIXULRuntime).processType;
    if (processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT)
      return Cr.NS_ERROR_NOT_IMPLEMENTED;

    return (this._instance = new HandlerService());
  }
};

HandlerService.prototype = {
  //**************************************************************************//
  // XPCOM Plumbing

  classID:          Components.ID("{32314cc8-22f7-4f7f-a645-1a45453ba6a6}"),
  QueryInterface:   XPCOMUtils.generateQI([Ci.nsIHandlerService]),
  _xpcom_factory: HandlerServiceFactory,

  //**************************************************************************//
  // Initialization & Destruction
  
  _init: function HS__init() {
    // Observe profile-before-change so we can switch to the datasource
    // in the new profile when the user changes profiles.
    this._observerSvc.addObserver(this, "profile-before-change");

    // Observe xpcom-shutdown so we can remove these observers
    // when the application shuts down.
    this._observerSvc.addObserver(this, "xpcom-shutdown");

    // Observe profile-do-change so that non-default profiles get upgraded too
    this._observerSvc.addObserver(this, "profile-do-change");

    // Observe handlersvc-rdf-replace so we can switch to the datasource
    this._observerSvc.addObserver(this, "handlersvc-rdf-replace");
  },

  _updateDB: function HS__updateDB() {
    try {
      var defaultHandlersVersion = this._datastoreDefaultHandlersVersion;
    } catch(ex) {
      // accessing the datastore failed, we can't update anything
      return;
    }

    try {
      // if we don't have the current version of the default prefs for
      // this locale, inject any new default handers into the datastore
      if (defaultHandlersVersion < this._prefsDefaultHandlersVersion) {

        // set the new version first so that if we recurse we don't
        // call _injectNewDefaults several times
        this._datastoreDefaultHandlersVersion =
          this._prefsDefaultHandlersVersion;
        this._injectNewDefaults();
      } 
    } catch (ex) {
      // if injecting the defaults failed, set the version back to the
      // previous value
      this._datastoreDefaultHandlersVersion = defaultHandlersVersion;
    }
  },

  get _currentLocale() {
    const locSvc = Cc["@mozilla.org/intl/localeservice;1"].
                   getService(Ci.mozILocaleService);
    return locSvc.getAppLocaleAsLangTag();
  }, 

  _destroy: function HS__destroy() {
    this._observerSvc.removeObserver(this, "profile-before-change");
    this._observerSvc.removeObserver(this, "xpcom-shutdown");
    this._observerSvc.removeObserver(this, "profile-do-change");
    this._observerSvc.removeObserver(this, "handlersvc-rdf-replace");

    // XXX Should we also null references to all the services that get stored
    // by our memoizing getters in the Convenience Getters section?
  },

  _onProfileChange: function HS__onProfileChange() {
    // Lose our reference to the datasource so we reacquire it
    // from the new profile the next time we need it.
    this.__ds = null;
  },

  _isInHandlerArray: function HS__isInHandlerArray(aArray, aHandler) {
    var enumerator = aArray.enumerate();
    while (enumerator.hasMoreElements()) {
      let handler = enumerator.getNext();
      handler.QueryInterface(Ci.nsIHandlerApp);
      if (handler.equals(aHandler))
        return true;
    }
    
    return false;
  },

  // note that this applies to the current locale only 
  get _datastoreDefaultHandlersVersion() {
    var version = this._getValue("urn:root", NC_NS + this._currentLocale +
                                             "_" + DEFAULT_HANDLERS_VERSION);
    
    return version ? version : -1;
  },

  set _datastoreDefaultHandlersVersion(aNewVersion) {
    return this._setLiteral("urn:root", NC_NS + this._currentLocale + "_" + 
                            DEFAULT_HANDLERS_VERSION, aNewVersion);
  },

  get _prefsDefaultHandlersVersion() {
    // get handler service pref branch
    var prefSvc = Cc["@mozilla.org/preferences-service;1"].
                  getService(Ci.nsIPrefService);
    var handlerSvcBranch = prefSvc.getBranch("gecko.handlerService.");

    // get the version of the preferences for this locale
    return Number(handlerSvcBranch.
                  getComplexValue("defaultHandlersVersion", 
                                  Ci.nsIPrefLocalizedString).data);
  },
  
  _injectNewDefaults: function HS__injectNewDefaults() {
    // get handler service pref branch
    var prefSvc = Cc["@mozilla.org/preferences-service;1"].
                  getService(Ci.nsIPrefService);

    let schemesPrefBranch = prefSvc.getBranch("gecko.handlerService.schemes.");
    let schemePrefList = schemesPrefBranch.getChildList("");

    var schemes = {};

    // read all the scheme prefs into a hash
    for (var schemePrefName of schemePrefList) {

      let [scheme, handlerNumber, attribute] = schemePrefName.split(".");

      try {
        var attrData =
          schemesPrefBranch.getComplexValue(schemePrefName,
                                            Ci.nsIPrefLocalizedString).data;
        if (!(scheme in schemes))
          schemes[scheme] = {};
  
        if (!(handlerNumber in schemes[scheme]))
          schemes[scheme][handlerNumber] = {};
        
        schemes[scheme][handlerNumber][attribute] = attrData;
      } catch (ex) {}
    }

    let protoSvc = Cc["@mozilla.org/uriloader/external-protocol-service;1"].
                   getService(Ci.nsIExternalProtocolService);
    for (var scheme in schemes) {

      // This clause is essentially a reimplementation of 
      // nsIExternalProtocolHandlerService.getProtocolHandlerInfo().
      // Necessary because calling that from here would make XPConnect barf
      // when getService tried to re-enter the constructor for this
      // service.
      let osDefaultHandlerFound = {};
      let protoInfo = protoSvc.getProtocolHandlerInfoFromOS(scheme, 
                               osDefaultHandlerFound);
      
      if (this.exists(protoInfo))
        this.fillHandlerInfo(protoInfo, null);
      else
        protoSvc.setProtocolHandlerDefaults(protoInfo, 
                                            osDefaultHandlerFound.value);

      // cache the possible handlers to avoid extra xpconnect traversals.      
      let possibleHandlers = protoInfo.possibleApplicationHandlers;

      for (let handlerNumber in schemes[scheme]) {
        let handlerPrefs = schemes[scheme][handlerNumber];
        let handlerApp = Cc["@mozilla.org/uriloader/web-handler-app;1"].
                         createInstance(Ci.nsIWebHandlerApp);

        handlerApp.uriTemplate = handlerPrefs.uriTemplate;
        handlerApp.name = handlerPrefs.name;                

        if (!this._isInHandlerArray(possibleHandlers, handlerApp)) {
          possibleHandlers.appendElement(handlerApp);
        }
      }

      this.store(protoInfo);
    }
  },

  //**************************************************************************//
  // nsIObserver
  
  observe: function HS__observe(subject, topic, data) {
    switch(topic) {
      case "profile-before-change":
        this._onProfileChange();
        break;
      case "xpcom-shutdown":
        this._destroy();
        break;
      case "profile-do-change":
        this._updateDB();
        break;
      case "handlersvc-rdf-replace":
        if (this.__ds) {
          this._rdf.UnregisterDataSource(this.__ds);
          this.__ds = null;
        }
        this._observerSvc.notifyObservers(null, "handlersvc-rdf-replace-complete");
        break;
    }
  },


  //**************************************************************************//
  // nsIHandlerService

  enumerate: function HS_enumerate() {
    var handlers = Cc["@mozilla.org/array;1"].
                   createInstance(Ci.nsIMutableArray);
    this._appendHandlers(handlers, CLASS_MIMEINFO);
    this._appendHandlers(handlers, CLASS_PROTOCOLINFO);
    return handlers.enumerate();
  },

  fillHandlerInfo: function HS_fillHandlerInfo(aHandlerInfo, aOverrideType) {
    var type = aOverrideType || aHandlerInfo.type;
    var typeID = this._getTypeID(this._getClass(aHandlerInfo), type);

    // Determine whether or not information about this handler is available
    // in the datastore by looking for its "value" property, which stores its
    // type and should always be present.
    if (!this._hasValue(typeID, NC_VALUE))
      throw new Components.Exception("handlerSvc fillHandlerInfo: don't know this type",
                                     Cr.NS_ERROR_NOT_AVAILABLE);

    // Note: for historical reasons, we don't actually check that the type
    // record has a "handlerProp" property referencing the info record.  It's
    // unclear whether or not we should start doing this check; perhaps some
    // legacy datasources don't have such references.
    var infoID = this._getInfoID(this._getClass(aHandlerInfo), type);

    aHandlerInfo.preferredAction = this._retrievePreferredAction(infoID);

    var preferredHandlerID =
      this._getPreferredHandlerID(this._getClass(aHandlerInfo), type);

    // Retrieve the preferred handler.
    // Note: for historical reasons, we don't actually check that the info
    // record has an "externalApplication" property referencing the preferred
    // handler record.  It's unclear whether or not we should start doing
    // this check; perhaps some legacy datasources don't have such references.
    aHandlerInfo.preferredApplicationHandler =
      this._retrieveHandlerApp(preferredHandlerID);

    // Fill the array of possible handlers with the ones in the datastore.
    this._fillPossibleHandlers(infoID,
                               aHandlerInfo.possibleApplicationHandlers,
                               aHandlerInfo.preferredApplicationHandler);

    // If we have an "always ask" flag stored in the RDF, always use its
    // value. Otherwise, use the default value stored in the pref service.
    var alwaysAsk;
    if (this._hasValue(infoID, NC_ALWAYS_ASK)) {
      alwaysAsk = (this._getValue(infoID, NC_ALWAYS_ASK) != "false");
    } else {
      var prefSvc = Cc["@mozilla.org/preferences-service;1"].
                    getService(Ci.nsIPrefService);
      var prefBranch = prefSvc.getBranch("network.protocol-handler.");
      // If neither of the prefs exists, be paranoid and prompt.
      alwaysAsk =
        prefBranch.getBoolPref("warn-external." + type,
                               prefBranch.getBoolPref("warn-external-default",
                                                      true));
    }
    aHandlerInfo.alwaysAskBeforeHandling = alwaysAsk;

    // If the object represents a MIME type handler, then also retrieve
    // any file extensions.
    if (aHandlerInfo instanceof Ci.nsIMIMEInfo)
      for (let fileExtension of this._retrieveFileExtensions(typeID))
        aHandlerInfo.appendExtension(fileExtension);
  },

  store: function HS_store(aHandlerInfo) {
    // FIXME: when we switch from RDF to something with transactions (like
    // SQLite), enclose the following changes in a transaction so they all
    // get rolled back if any of them fail and we don't leave the datastore
    // in an inconsistent state.

    this._ensureRecordsForType(aHandlerInfo);
    this._storePreferredAction(aHandlerInfo);
    this._storePreferredHandler(aHandlerInfo);
    this._storePossibleHandlers(aHandlerInfo);
    this._storeAlwaysAsk(aHandlerInfo);
    this._storeExtensions(aHandlerInfo);

    // Write the changes to the database immediately so we don't lose them
    // if the application crashes.
    if (this._ds instanceof Ci.nsIRDFRemoteDataSource)
      this._ds.Flush();
  },

  exists: function HS_exists(aHandlerInfo) {
    var found;

    try {
      var typeID = this._getTypeID(this._getClass(aHandlerInfo), aHandlerInfo.type);
      found = this._hasLiteralAssertion(typeID, NC_VALUE, aHandlerInfo.type);
    } catch (e) {
      // If the RDF threw (eg, corrupt file), treat as nonexistent.
      found = false;
    }

    return found;
  },

  remove: function HS_remove(aHandlerInfo) {
    var preferredHandlerID =
      this._getPreferredHandlerID(this._getClass(aHandlerInfo), aHandlerInfo.type);
    this._removeAssertions(preferredHandlerID);

    var infoID = this._getInfoID(this._getClass(aHandlerInfo), aHandlerInfo.type);

    // Get a list of possible handlers.  After we have removed the info record,
    // we'll check if any other info records reference these handlers, and we'll
    // remove the handler records that aren't referenced by other info records.
    var possibleHandlerIDs = [];
    var possibleHandlerTargets = this._getTargets(infoID, NC_POSSIBLE_APP);
    while (possibleHandlerTargets.hasMoreElements()) {
      let possibleHandlerTarget = possibleHandlerTargets.getNext();
      // Note: possibleHandlerTarget should always be an nsIRDFResource.
      // The conditional is just here in case of a corrupt RDF datasource.
      if (possibleHandlerTarget instanceof Ci.nsIRDFResource)
        possibleHandlerIDs.push(possibleHandlerTarget.ValueUTF8);
    }

    // Remove the info record.
    this._removeAssertions(infoID);

    // Now that we've removed the info record, remove any possible handlers
    // that aren't referenced by other info records.
    for (let possibleHandlerID of possibleHandlerIDs)
      if (!this._existsResourceTarget(NC_POSSIBLE_APP, possibleHandlerID))
        this._removeAssertions(possibleHandlerID);

    var typeID = this._getTypeID(this._getClass(aHandlerInfo), aHandlerInfo.type);
    this._removeAssertions(typeID);

    // Now that there's no longer a handler for this type, remove the type
    // from the list of types for which there are known handlers.
    var typeList = this._ensureAndGetTypeList(this._getClass(aHandlerInfo));
    var type = this._rdf.GetResource(typeID);
    var typeIndex = typeList.IndexOf(type);
    if (typeIndex != -1)
      typeList.RemoveElementAt(typeIndex, true);

    // Write the changes to the database immediately so we don't lose them
    // if the application crashes.
    // XXX If we're removing a bunch of handlers at once, will flushing
    // after every removal cause a significant performance hit?
    if (this._ds instanceof Ci.nsIRDFRemoteDataSource)
      this._ds.Flush();
  },

  getTypeFromExtension: function HS_getTypeFromExtension(aFileExtension) {
    var fileExtension = aFileExtension.toLowerCase();
    var typeID;

    // See bug 1100069 for why we want to fail gracefully and silently here.
    try {
      this._ds;
    } catch (ex) {
      Components.returnCode = Cr.NS_ERROR_NOT_AVAILABLE;
      return;
    }

    if (this._existsLiteralTarget(NC_FILE_EXTENSIONS, fileExtension))
      typeID = this._getSourceForLiteral(NC_FILE_EXTENSIONS, fileExtension);

    if (typeID && this._hasValue(typeID, NC_VALUE)) {
      let type = this._getValue(typeID, NC_VALUE);
      if (type == "")
        throw Cr.NS_ERROR_FAILURE;
      return type;
    }

    return "";
  },


  //**************************************************************************//
  // Retrieval Methods

  /**
   * Retrieve the preferred action for the info record with the given ID.
   *
   * @param aInfoID  {string}  the info record ID
   *
   * @returns  {integer}  the preferred action enumeration value
   */
  _retrievePreferredAction: function HS__retrievePreferredAction(aInfoID) {
    if (this._getValue(aInfoID, NC_SAVE_TO_DISK) == "true")
      return Ci.nsIHandlerInfo.saveToDisk;
    
    if (this._getValue(aInfoID, NC_USE_SYSTEM_DEFAULT) == "true")
      return Ci.nsIHandlerInfo.useSystemDefault;
    
    if (this._getValue(aInfoID, NC_HANDLE_INTERNALLY) == "true")
      return Ci.nsIHandlerInfo.handleInternally;

    return Ci.nsIHandlerInfo.useHelperApp;
  },

  /**
   * Fill an array of possible handlers with the handlers for the given info ID.
   *
   * @param aInfoID            {string}           the ID of the info record
   * @param aPossibleHandlers  {nsIMutableArray}  the array of possible handlers
   * @param aPreferredHandler  {nsIHandlerApp}    the preferred handler, if any
   */
  _fillPossibleHandlers: function HS__fillPossibleHandlers(aInfoID,
                                                           aPossibleHandlers,
                                                           aPreferredHandler) {
    // The set of possible handlers should include the preferred handler,
    // but legacy datastores (from before we added possible handlers) won't
    // include the preferred handler, so check if it's included as we build
    // the list of handlers, and, if it's not included, add it to the list.
    if (aPreferredHandler)
      aPossibleHandlers.appendElement(aPreferredHandler);

    var possibleHandlerTargets = this._getTargets(aInfoID, NC_POSSIBLE_APP);

    while (possibleHandlerTargets.hasMoreElements()) {
      let possibleHandlerTarget = possibleHandlerTargets.getNext();
      if (!(possibleHandlerTarget instanceof Ci.nsIRDFResource))
        continue;

      let possibleHandlerID = possibleHandlerTarget.ValueUTF8;
      let possibleHandler = this._retrieveHandlerApp(possibleHandlerID);
      if (possibleHandler && (!aPreferredHandler ||
                              !possibleHandler.equals(aPreferredHandler)))
        aPossibleHandlers.appendElement(possibleHandler);
    }
  },

  /**
   * Retrieve the handler app object with the given ID.
   *
   * @param aHandlerAppID  {string}  the ID of the handler app to retrieve
   *
   * @returns  {nsIHandlerApp}  the handler app, if any; otherwise null
   */
  _retrieveHandlerApp: function HS__retrieveHandlerApp(aHandlerAppID) {
    var handlerApp;

    // If it has a path, it's a local handler; otherwise, it's a web handler.
    if (this._hasValue(aHandlerAppID, NC_PATH)) {
      let executable =
        this._getFileWithPath(this._getValue(aHandlerAppID, NC_PATH));
      if (!executable)
        return null;

      handlerApp = Cc["@mozilla.org/uriloader/local-handler-app;1"].
                   createInstance(Ci.nsILocalHandlerApp);
      handlerApp.executable = executable;
    }
    else if (this._hasValue(aHandlerAppID, NC_URI_TEMPLATE)) {
      let uriTemplate = this._getValue(aHandlerAppID, NC_URI_TEMPLATE);
      if (!uriTemplate)
        return null;

      handlerApp = Cc["@mozilla.org/uriloader/web-handler-app;1"].
                   createInstance(Ci.nsIWebHandlerApp);
      handlerApp.uriTemplate = uriTemplate;
    }
    else if (this._hasValue(aHandlerAppID, NC_SERVICE)) {
      let service = this._getValue(aHandlerAppID, NC_SERVICE);
      if (!service)
        return null;
      
      let method = this._getValue(aHandlerAppID, NC_METHOD);
      if (!method)
        return null;
      
      let objpath = this._getValue(aHandlerAppID, NC_OBJPATH);
      if (!objpath)
        return null;
      
      let iface = this._getValue(aHandlerAppID, NC_INTERFACE);
      if (!iface)
        return null;
      
      handlerApp = Cc["@mozilla.org/uriloader/dbus-handler-app;1"].
                   createInstance(Ci.nsIDBusHandlerApp);
      handlerApp.service   = service;
      handlerApp.method    = method;
      handlerApp.objectPath   = objpath;
      handlerApp.dBusInterface = iface;
      
    }
    else
      return null;

    handlerApp.name = this._getValue(aHandlerAppID, NC_PRETTY_NAME);

    return handlerApp;
  },

  /*
   * Retrieve file extensions, if any, for the MIME type with the given type ID.
   *
   * @param aTypeID  {string}  the type record ID
   */
  _retrieveFileExtensions: function HS__retrieveFileExtensions(aTypeID) {
    var fileExtensions = [];

    var fileExtensionTargets = this._getTargets(aTypeID, NC_FILE_EXTENSIONS);

    while (fileExtensionTargets.hasMoreElements()) {
      let fileExtensionTarget = fileExtensionTargets.getNext();
      if (fileExtensionTarget instanceof Ci.nsIRDFLiteral &&
          fileExtensionTarget.Value != "")
        fileExtensions.push(fileExtensionTarget.Value);
    }

    return fileExtensions;
  },

  /**
   * Get the file with the given path.  This is not as simple as merely
   * initializing a local file object with the path, because the path might be
   * relative to the current process directory, in which case we have to
   * construct a path starting from that directory.
   *
   * @param aPath  {string}  a path to a file
   *
   * @returns {nsILocalFile} the file, or null if the file does not exist
   */
  _getFileWithPath: function HS__getFileWithPath(aPath) {
    var file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);

    try {
      file.initWithPath(aPath);

      if (file.exists())
        return file;
    }
    catch(ex) {
      // Note: for historical reasons, we don't actually check to see
      // if the exception is NS_ERROR_FILE_UNRECOGNIZED_PATH, which is what
      // nsILocalFile::initWithPath throws when a path is relative.

      file = this._dirSvc.get("XCurProcD", Ci.nsIFile);

      try {
        file.append(aPath);
        if (file.exists())
          return file;
      }
      catch(ex) {}
    }

    return null;
  },


  //**************************************************************************//
  // Storage Methods

  _storePreferredAction: function HS__storePreferredAction(aHandlerInfo) {
    var infoID = this._getInfoID(this._getClass(aHandlerInfo), aHandlerInfo.type);

    switch(aHandlerInfo.preferredAction) {
      case Ci.nsIHandlerInfo.saveToDisk:
        this._setLiteral(infoID, NC_SAVE_TO_DISK, "true");
        this._removeTarget(infoID, NC_HANDLE_INTERNALLY);
        this._removeTarget(infoID, NC_USE_SYSTEM_DEFAULT);
        break;

      case Ci.nsIHandlerInfo.handleInternally:
        this._setLiteral(infoID, NC_HANDLE_INTERNALLY, "true");
        this._removeTarget(infoID, NC_SAVE_TO_DISK);
        this._removeTarget(infoID, NC_USE_SYSTEM_DEFAULT);
        break;

      case Ci.nsIHandlerInfo.useSystemDefault:
        this._setLiteral(infoID, NC_USE_SYSTEM_DEFAULT, "true");
        this._removeTarget(infoID, NC_SAVE_TO_DISK);
        this._removeTarget(infoID, NC_HANDLE_INTERNALLY);
        break;

      // This value is indicated in the datastore either by the absence of
      // the three properties or by setting them all "false".  Of these two
      // options, the former seems preferable, because it reduces the size
      // of the RDF file and thus the amount of stuff we have to parse.
      case Ci.nsIHandlerInfo.useHelperApp:
      default:
        this._removeTarget(infoID, NC_SAVE_TO_DISK);
        this._removeTarget(infoID, NC_HANDLE_INTERNALLY);
        this._removeTarget(infoID, NC_USE_SYSTEM_DEFAULT);
        break;
    }
  },

  _handlerAppIsUnknownType: function HS__handlerAppIsUnknownType(aHandlerApp) {
    if (aHandlerApp instanceof Ci.nsILocalHandlerApp ||
        aHandlerApp instanceof Ci.nsIWebHandlerApp ||
        aHandlerApp instanceof Ci.nsIDBusHandlerApp) {
      return false;
    } else {
      return true;
    }
  },


  _storePreferredHandler: function HS__storePreferredHandler(aHandlerInfo) {
    var infoID = this._getInfoID(this._getClass(aHandlerInfo), aHandlerInfo.type);
    var handlerID =
      this._getPreferredHandlerID(this._getClass(aHandlerInfo), aHandlerInfo.type);

    var handler = aHandlerInfo.preferredApplicationHandler;

    if (handler) {
      // If the handlerApp is an unknown type, ignore it.
      // Android default application handler is the case.
      if (this._handlerAppIsUnknownType(handler)) {
        return;
      }
      this._storeHandlerApp(handlerID, handler);

      // Make this app be the preferred app for the handler info.
      //
      // Note: nsExternalHelperAppService::FillContentHandlerProperties ignores
      // this setting and instead identifies the preferred app as the resource
      // whose URI follows the pattern urn:<class>:externalApplication:<type>.
      // But the old downloadactions.js code used to set this property, so just
      // in case there is still some code somewhere that relies on its presence,
      // we set it here.
      this._setResource(infoID, NC_PREFERRED_APP, handlerID);
    }
    else {
      // There isn't a preferred handler.  Remove the existing record for it,
      // if any.
      this._removeTarget(infoID, NC_PREFERRED_APP);
      this._removeAssertions(handlerID);
    }
  },

  /**
   * Store the list of possible handler apps for the content type represented
   * by the given handler info object.
   *
   * @param aHandlerInfo  {nsIHandlerInfo}  the handler info object
   */
  _storePossibleHandlers: function HS__storePossibleHandlers(aHandlerInfo) {
    var infoID = this._getInfoID(this._getClass(aHandlerInfo), aHandlerInfo.type);

    // First, retrieve the set of handler apps currently stored for the type,
    // keeping track of their IDs in a hash that we'll use to determine which
    // ones are no longer valid and should be removed.
    var currentHandlerApps = {};
    var currentHandlerTargets = this._getTargets(infoID, NC_POSSIBLE_APP);
    while (currentHandlerTargets.hasMoreElements()) {
      let handlerApp = currentHandlerTargets.getNext();
      if (handlerApp instanceof Ci.nsIRDFResource) {
        let handlerAppID = handlerApp.ValueUTF8;
        currentHandlerApps[handlerAppID] = true;
      }
    }

    // Next, store any new handler apps.
    var newHandlerApps =
      aHandlerInfo.possibleApplicationHandlers.enumerate();
    while (newHandlerApps.hasMoreElements()) {
      let handlerApp =
        newHandlerApps.getNext().QueryInterface(Ci.nsIHandlerApp);
      // If the handlerApp is an unknown type, ignore it.
      // Android default application handler is the case.
      if (this._handlerAppIsUnknownType(handlerApp)) {
        continue;
      }
      let handlerAppID = this._getPossibleHandlerAppID(handlerApp);
      if (!this._hasResourceAssertion(infoID, NC_POSSIBLE_APP, handlerAppID)) {
        this._storeHandlerApp(handlerAppID, handlerApp);
        this._addResourceAssertion(infoID, NC_POSSIBLE_APP, handlerAppID);
      }
      delete currentHandlerApps[handlerAppID];
    }

    // Finally, remove any old handler apps that aren't being used anymore,
    // and if those handler apps aren't being used by any other type either,
    // then completely remove their record from the datastore so we don't
    // leave it clogged up with information about handler apps we don't care
    // about anymore.
    for (let handlerAppID in currentHandlerApps) {
      this._removeResourceAssertion(infoID, NC_POSSIBLE_APP, handlerAppID);
      if (!this._existsResourceTarget(NC_POSSIBLE_APP, handlerAppID))
        this._removeAssertions(handlerAppID);
    }
  },

  /**
   * Store the given handler app.
   *
   * Note: the reason this method takes the ID of the handler app in a param
   * is that the ID is different than it usually is when the handler app
   * in question is a preferred handler app, so this method can't just derive
   * the ID of the handler app by calling _getPossibleHandlerAppID, its callers
   * have to do that for it.
   *
   * @param aHandlerAppID {string}        the ID of the handler app to store
   * @param aHandlerApp   {nsIHandlerApp} the handler app to store
   */
  _storeHandlerApp: function HS__storeHandlerApp(aHandlerAppID, aHandlerApp) {
    aHandlerApp.QueryInterface(Ci.nsIHandlerApp);
    this._setLiteral(aHandlerAppID, NC_PRETTY_NAME, aHandlerApp.name);

    // In the case of the preferred handler, the handler ID could have been
    // used to refer to a different kind of handler in the past (i.e. either
    // a local hander or a web handler), so if the new handler is a local
    // handler, then we remove any web handler properties and vice versa.
    // This is unnecessary but harmless for possible handlers.

    if (aHandlerApp instanceof Ci.nsILocalHandlerApp) {
      this._setLiteral(aHandlerAppID, NC_PATH, aHandlerApp.executable.path);
      this._removeTarget(aHandlerAppID, NC_URI_TEMPLATE);
      this._removeTarget(aHandlerAppID, NC_METHOD);
      this._removeTarget(aHandlerAppID, NC_SERVICE);
      this._removeTarget(aHandlerAppID, NC_OBJPATH);
      this._removeTarget(aHandlerAppID, NC_INTERFACE);
    }
    else if(aHandlerApp instanceof Ci.nsIWebHandlerApp){
      aHandlerApp.QueryInterface(Ci.nsIWebHandlerApp);
      this._setLiteral(aHandlerAppID, NC_URI_TEMPLATE, aHandlerApp.uriTemplate);
      this._removeTarget(aHandlerAppID, NC_PATH);
      this._removeTarget(aHandlerAppID, NC_METHOD);
      this._removeTarget(aHandlerAppID, NC_SERVICE);
      this._removeTarget(aHandlerAppID, NC_OBJPATH);
      this._removeTarget(aHandlerAppID, NC_INTERFACE);
    }
    else if(aHandlerApp instanceof Ci.nsIDBusHandlerApp){
      aHandlerApp.QueryInterface(Ci.nsIDBusHandlerApp);
      this._setLiteral(aHandlerAppID, NC_SERVICE, aHandlerApp.service);
      this._setLiteral(aHandlerAppID, NC_METHOD, aHandlerApp.method);
      this._setLiteral(aHandlerAppID, NC_OBJPATH, aHandlerApp.objectPath);
      this._setLiteral(aHandlerAppID, NC_INTERFACE, aHandlerApp.dBusInterface);
      this._removeTarget(aHandlerAppID, NC_PATH);
      this._removeTarget(aHandlerAppID, NC_URI_TEMPLATE);
    }
    else {
      throw "unknown handler type";
    }
	
  },

  _storeAlwaysAsk: function HS__storeAlwaysAsk(aHandlerInfo) {
    var infoID = this._getInfoID(this._getClass(aHandlerInfo), aHandlerInfo.type);
    this._setLiteral(infoID,
                     NC_ALWAYS_ASK,
                     aHandlerInfo.alwaysAskBeforeHandling ? "true" : "false");
  },

  _storeExtensions: function HS__storeExtensions(aHandlerInfo) {
    if (aHandlerInfo instanceof Ci.nsIMIMEInfo) {
      var typeID = this._getTypeID(this._getClass(aHandlerInfo), aHandlerInfo.type);
      var extEnum = aHandlerInfo.getFileExtensions();
      while (extEnum.hasMore()) {
        let ext = extEnum.getNext().toLowerCase();
        if (!this._hasLiteralAssertion(typeID, NC_FILE_EXTENSIONS, ext)) {
          this._setLiteral(typeID, NC_FILE_EXTENSIONS, ext);
        }
      }
    }
  },


  //**************************************************************************//
  // Convenience Getters

  // Observer Service
  __observerSvc: null,
  get _observerSvc() {
    if (!this.__observerSvc)
      this.__observerSvc =
        Cc["@mozilla.org/observer-service;1"].
        getService(Ci.nsIObserverService);
    return this.__observerSvc;
  },

  // Directory Service
  __dirSvc: null,
  get _dirSvc() {
    if (!this.__dirSvc)
      this.__dirSvc =
        Cc["@mozilla.org/file/directory_service;1"].
        getService(Ci.nsIProperties);
    return this.__dirSvc;
  },

  // MIME Service
  __mimeSvc: null,
  get _mimeSvc() {
    if (!this.__mimeSvc)
      this.__mimeSvc =
        Cc["@mozilla.org/mime;1"].
        getService(Ci.nsIMIMEService);
    return this.__mimeSvc;
  },

  // Protocol Service
  __protocolSvc: null,
  get _protocolSvc() {
    if (!this.__protocolSvc)
      this.__protocolSvc =
        Cc["@mozilla.org/uriloader/external-protocol-service;1"].
        getService(Ci.nsIExternalProtocolService);
    return this.__protocolSvc;
  },

  // RDF Service
  __rdf: null,
  get _rdf() {
    if (!this.__rdf)
      this.__rdf = Cc["@mozilla.org/rdf/rdf-service;1"].
                   getService(Ci.nsIRDFService);
    return this.__rdf;
  },

  // RDF Container Utils
  __containerUtils: null,
  get _containerUtils() {
    if (!this.__containerUtils)
      this.__containerUtils = Cc["@mozilla.org/rdf/container-utils;1"].
                              getService(Ci.nsIRDFContainerUtils);
    return this.__containerUtils;
  },

  // RDF datasource containing content handling config (i.e. mimeTypes.rdf)
  __ds: null,
  get _ds() {
    if (!this.__ds) {
      var file = this._dirSvc.get("ProfD", Ci.nsIFile);
      file.append("mimeTypes.rdf");
      // FIXME: make this a memoizing getter if we use it anywhere else.
      var ioService = Cc["@mozilla.org/network/io-service;1"].
                      getService(Ci.nsIIOService);
      var fileHandler = ioService.getProtocolHandler("file").
                        QueryInterface(Ci.nsIFileProtocolHandler);
      this.__ds =
        this._rdf.GetDataSourceBlocking(fileHandler.getURLSpecFromFile(file));
      // do any necessary updating of the datastore
      this._updateDB();
    }

    return this.__ds;
  },


  //**************************************************************************//
  // Datastore Utils

  /**
   * Get the string identifying whether this is a MIME or a protocol handler.
   * This string is used in the URI IDs of various RDF properties.
   * 
   * @param aHandlerInfo {nsIHandlerInfo} the handler for which to get the class
   * 
   * @returns {string} the class
   */
  _getClass: function HS__getClass(aHandlerInfo) {
    if (aHandlerInfo instanceof Ci.nsIMIMEInfo)
      return CLASS_MIMEINFO;
    else
      return CLASS_PROTOCOLINFO;
  },

  /**
   * Return the unique identifier for a content type record, which stores
   * the value field plus a reference to the content type's handler info record.
   *
   * |urn:<class>:<type>|
   *
   * XXX: should this be a property of nsIHandlerInfo?
   *
   * @param aClass {string} the class (CLASS_MIMEINFO or CLASS_PROTOCOLINFO)
   * @param aType  {string} the type (a MIME type or protocol scheme)
   *
   * @returns {string} the ID
   */
  _getTypeID: function HS__getTypeID(aClass, aType) {
    return "urn:" + aClass + ":" + aType;
  },

  /**
   * Return the unique identifier for a handler info record, which stores
   * the preferredAction and alwaysAsk fields plus a reference to the preferred
   * handler app.  Roughly equivalent to the nsIHandlerInfo interface.
   *
   * |urn:<class>:handler:<type>|
   *
   * FIXME: the type info record should be merged into the type record,
   * since there's a one to one relationship between them, and this record
   * merely stores additional attributes of a content type.
   *
   * @param aClass {string} the class (CLASS_MIMEINFO or CLASS_PROTOCOLINFO)
   * @param aType  {string} the type (a MIME type or protocol scheme)
   *
   * @returns {string} the ID
   */
  _getInfoID: function HS__getInfoID(aClass, aType) {
    return "urn:" + aClass + ":handler:" + aType;
  },

  /**
   * Return the unique identifier for a preferred handler record, which stores
   * information about the preferred handler for a given content type, including
   * its human-readable name and the path to its executable (for a local app)
   * or its URI template (for a web app).
   * 
   * |urn:<class>:externalApplication:<type>|
   *
   * XXX: should this be a property of nsIHandlerApp?
   *
   * FIXME: this should be an arbitrary ID, and we should retrieve it from
   * the datastore for a given content type via the NC:ExternalApplication
   * property rather than looking for a specific ID, so a handler doesn't
   * have to change IDs when it goes from being a possible handler to being
   * the preferred one (once we support possible handlers).
   * 
   * @param aClass {string} the class (CLASS_MIMEINFO or CLASS_PROTOCOLINFO)
   * @param aType  {string} the type (a MIME type or protocol scheme)
   * 
   * @returns {string} the ID
   */
  _getPreferredHandlerID: function HS__getPreferredHandlerID(aClass, aType) {
    return "urn:" + aClass + ":externalApplication:" + aType;
  },

  /**
   * Return the unique identifier for a handler app record, which stores
   * information about a possible handler for one or more content types,
   * including its human-readable name and the path to its executable (for a
   * local app) or its URI template (for a web app).
   *
   * Note: handler app IDs for preferred handlers are different.  For those,
   * see the _getPreferredHandlerID method.
   *
   * @param aHandlerApp  {nsIHandlerApp}   the handler app object
   */
  _getPossibleHandlerAppID: function HS__getPossibleHandlerAppID(aHandlerApp) {
    var handlerAppID = "urn:handler:";

    if (aHandlerApp instanceof Ci.nsILocalHandlerApp)
      handlerAppID += "local:" + aHandlerApp.executable.path;
    else if(aHandlerApp instanceof Ci.nsIWebHandlerApp){
      aHandlerApp.QueryInterface(Ci.nsIWebHandlerApp);
      handlerAppID += "web:" + aHandlerApp.uriTemplate;
    }
    else if(aHandlerApp instanceof Ci.nsIDBusHandlerApp){
      aHandlerApp.QueryInterface(Ci.nsIDBusHandlerApp);
      handlerAppID += "dbus:" + aHandlerApp.service + " " + aHandlerApp.method + " " + aHandlerApp.uriTemplate;
    }else{
	throw "unknown handler type";
    }
    
    return handlerAppID;
  },

  /**
   * Get the list of types for the given class, creating the list if it doesn't
   * already exist. The class can be either CLASS_MIMEINFO or CLASS_PROTOCOLINFO
   * (i.e. the result of a call to _getClass).
   * 
   * |urn:<class>s|
   * |urn:<class>s:root|
   * 
   * @param aClass {string} the class for which to retrieve a list of types
   *
   * @returns {nsIRDFContainer} the list of types
   */
  _ensureAndGetTypeList: function HS__ensureAndGetTypeList(aClass) {
    var source = this._rdf.GetResource("urn:" + aClass + "s");
    var property =
      this._rdf.GetResource(aClass == CLASS_MIMEINFO ? NC_MIME_TYPES
                                                     : NC_PROTOCOL_SCHEMES);
    var target = this._rdf.GetResource("urn:" + aClass + "s:root");

    // Make sure we have an arc from the source to the target.
    if (!this._ds.HasAssertion(source, property, target, true))
      this._ds.Assert(source, property, target, true);

    // Make sure the target is a container.
    if (!this._containerUtils.IsContainer(this._ds, target))
      this._containerUtils.MakeSeq(this._ds, target);

    // Get the type list as an RDF container.
    var typeList = Cc["@mozilla.org/rdf/container;1"].
                   createInstance(Ci.nsIRDFContainer);
    typeList.Init(this._ds, target);

    return typeList;
  },

  /**
   * Make sure there are records in the datasource for the given content type
   * by creating them if they don't already exist.  We have to do this before
   * storing any specific data, because we can't assume the presence
   * of the records (the nsIHandlerInfo object might have been created
   * from the OS), and the records have to all be there in order for the helper
   * app service to properly construct an nsIHandlerInfo object for the type.
   *
   * Based on old downloadactions.js::_ensureMIMERegistryEntry.
   *
   * @param aHandlerInfo {nsIHandlerInfo} the type to make sure has a record
   */
  _ensureRecordsForType: function HS__ensureRecordsForType(aHandlerInfo) {
    // Get the list of types.
    var typeList = this._ensureAndGetTypeList(this._getClass(aHandlerInfo));

    // If there's already a record in the datastore for this type, then we
    // don't need to do anything more.
    var typeID = this._getTypeID(this._getClass(aHandlerInfo), aHandlerInfo.type);
    var type = this._rdf.GetResource(typeID);
    if (typeList.IndexOf(type) != -1)
      return;

    // Create a basic type record for this type.
    typeList.AppendElement(type);
    this._setLiteral(typeID, NC_VALUE, aHandlerInfo.type);
    
    // Create a basic info record for this type.
    var infoID = this._getInfoID(this._getClass(aHandlerInfo), aHandlerInfo.type);
    this._setLiteral(infoID, NC_ALWAYS_ASK, "false");
    this._setResource(typeID, NC_HANDLER_INFO, infoID);
    // XXX Shouldn't we set preferredAction to useSystemDefault?
    // That's what it is if there's no record in the datastore; why should it
    // change to useHelperApp just because we add a record to the datastore?
    
    // Create a basic preferred handler record for this type.
    // XXX Not sure this is necessary, since preferred handlers are optional,
    // and nsExternalHelperAppService::FillHandlerInfoForTypeFromDS doesn't seem
    // to require the record , but downloadactions.js::_ensureMIMERegistryEntry
    // used to create it, so we'll do the same.
    var preferredHandlerID =
      this._getPreferredHandlerID(this._getClass(aHandlerInfo), aHandlerInfo.type);
    this._setLiteral(preferredHandlerID, NC_PATH, "");
    this._setResource(infoID, NC_PREFERRED_APP, preferredHandlerID);
  },

  /**
   * Append known handlers of the given class to the given array.  The class
   * can be either CLASS_MIMEINFO or CLASS_PROTOCOLINFO.
   *
   * @param aHandlers   {array} the array of handlers to append to
   * @param aClass      {string} the class for which to append handlers
   */
  _appendHandlers: function HS__appendHandlers(aHandlers, aClass) {
    var typeList = this._ensureAndGetTypeList(aClass);
    var enumerator = typeList.GetElements();

    while (enumerator.hasMoreElements()) {
      var element = enumerator.getNext();
      
      // This should never happen.  If it does, that means our datasource
      // is corrupted with type list entries that point to literal values
      // instead of resources.  If it does happen, let's just do our best
      // to recover by ignoring this entry and moving on to the next one.
      if (!(element instanceof Ci.nsIRDFResource))
        continue;

      // Get the value of the element's NC:value property, which contains
      // the MIME type or scheme for which we're retrieving a handler info.
      var type = this._getValue(element.ValueUTF8, NC_VALUE);
      if (!type)
        continue;

      var handler;
      if (typeList.Resource.ValueUTF8 == "urn:mimetypes:root")
        handler = this._mimeSvc.getFromTypeAndExtension(type, null);
      else
        handler = this._protocolSvc.getProtocolHandlerInfo(type);

      aHandlers.appendElement(handler);
    }
  },

  /**
   * Whether or not a property of an RDF source has a value.
   *
   * @param sourceURI   {string}  the URI of the source
   * @param propertyURI {string}  the URI of the property
   * @returns           {boolean} whether or not the property has a value
   */
  _hasValue: function HS__hasValue(sourceURI, propertyURI) {
    var source = this._rdf.GetResource(sourceURI);
    var property = this._rdf.GetResource(propertyURI);
    return this._ds.hasArcOut(source, property);
  },

  /**
   * Get the value of a property of an RDF source.
   *
   * @param sourceURI   {string} the URI of the source
   * @param propertyURI {string} the URI of the property
   * @returns           {string} the value of the property
   */
  _getValue: function HS__getValue(sourceURI, propertyURI) {
    var source = this._rdf.GetResource(sourceURI);
    var property = this._rdf.GetResource(propertyURI);

    var target = this._ds.GetTarget(source, property, true);

    if (!target)
      return null;
    
    if (target instanceof Ci.nsIRDFResource)
      return target.ValueUTF8;

    if (target instanceof Ci.nsIRDFLiteral)
      return target.Value;

    return null;
  },

  /**
   * Get all targets for the property of an RDF source.
   *
   * @param sourceURI   {string} the URI of the source
   * @param propertyURI {string} the URI of the property
   * 
   * @returns {nsISimpleEnumerator} an enumerator of targets
   */
  _getTargets: function HS__getTargets(sourceURI, propertyURI) {
    var source = this._rdf.GetResource(sourceURI);
    var property = this._rdf.GetResource(propertyURI);

    return this._ds.GetTargets(source, property, true);
  },

  /**
   * Set a property of an RDF source to a literal value.
   *
   * @param sourceURI   {string} the URI of the source
   * @param propertyURI {string} the URI of the property
   * @param value       {string} the literal value
   */
  _setLiteral: function HS__setLiteral(sourceURI, propertyURI, value) {
    var source = this._rdf.GetResource(sourceURI);
    var property = this._rdf.GetResource(propertyURI);
    var target = this._rdf.GetLiteral(value);
    
    this._setTarget(source, property, target);
  },

  /**
   * Set a property of an RDF source to a resource target.
   *
   * @param sourceURI   {string} the URI of the source
   * @param propertyURI {string} the URI of the property
   * @param targetURI   {string} the URI of the target
   */
  _setResource: function HS__setResource(sourceURI, propertyURI, targetURI) {
    var source = this._rdf.GetResource(sourceURI);
    var property = this._rdf.GetResource(propertyURI);
    var target = this._rdf.GetResource(targetURI);
    
    this._setTarget(source, property, target);
  },

  /**
   * Assert an arc into the RDF datasource if there is no arc with the given
   * source and property; otherwise, if there is already an existing arc,
   * change it to point to the given target. _setLiteral and _setResource
   * call this after converting their string arguments into resources
   * and literals, and most callers should call one of those two methods
   * instead of this one.
   *
   * @param source    {nsIRDFResource}  the source
   * @param property  {nsIRDFResource}  the property
   * @param target    {nsIRDFNode}      the target
   */
  _setTarget: function HS__setTarget(source, property, target) {
    if (this._ds.hasArcOut(source, property)) {
      var oldTarget = this._ds.GetTarget(source, property, true);
      this._ds.Change(source, property, oldTarget, target);
    }
    else
      this._ds.Assert(source, property, target, true);
  },

  /**
   * Assert that a property of an RDF source has a resource target.
   * 
   * The difference between this method and _setResource is that this one adds
   * an assertion even if one already exists, which allows its callers to make
   * sets of assertions (i.e. to set a property to multiple targets).
   *
   * @param sourceURI   {string} the URI of the source
   * @param propertyURI {string} the URI of the property
   * @param targetURI   {string} the URI of the target
   */
  _addResourceAssertion: function HS__addResourceAssertion(sourceURI,
                                                           propertyURI,
                                                           targetURI) {
    var source = this._rdf.GetResource(sourceURI);
    var property = this._rdf.GetResource(propertyURI);
    var target = this._rdf.GetResource(targetURI);
    
    this._ds.Assert(source, property, target, true);
  },

  /**
   * Remove an assertion with a resource target.
   *
   * @param sourceURI   {string} the URI of the source
   * @param propertyURI {string} the URI of the property
   * @param targetURI   {string} the URI of the target
   */
  _removeResourceAssertion: function HS__removeResourceAssertion(sourceURI,
                                                                 propertyURI,
                                                                 targetURI) {
    var source = this._rdf.GetResource(sourceURI);
    var property = this._rdf.GetResource(propertyURI);
    var target = this._rdf.GetResource(targetURI);

    this._ds.Unassert(source, property, target);
  },

  /**
   * Whether or not a property of an RDF source has a given resource target.
   * 
   * @param sourceURI   {string} the URI of the source
   * @param propertyURI {string} the URI of the property
   * @param targetURI   {string} the URI of the target
   *
   * @returns {boolean} whether or not there is such an assertion
   */
  _hasResourceAssertion: function HS__hasResourceAssertion(sourceURI,
                                                           propertyURI,
                                                           targetURI) {
    var source = this._rdf.GetResource(sourceURI);
    var property = this._rdf.GetResource(propertyURI);
    var target = this._rdf.GetResource(targetURI);

    return this._ds.HasAssertion(source, property, target, true);
  },

  /**
   * Whether or not a property of an RDF source has a given literal value.
   * 
   * @param sourceURI   {string} the URI of the source
   * @param propertyURI {string} the URI of the property
   * @param value       {string} the literal value
   *
   * @returns {boolean} whether or not there is such an assertion
   */
  _hasLiteralAssertion: function HS__hasLiteralAssertion(sourceURI,
                                                         propertyURI,
                                                         value) {
    var source = this._rdf.GetResource(sourceURI);
    var property = this._rdf.GetResource(propertyURI);
    var target = this._rdf.GetLiteral(value);

    return this._ds.HasAssertion(source, property, target, true);
  },

  /**
   * Whether or not there is an RDF source that has the given property set to
   * the given literal value.
   * 
   * @param propertyURI {string} the URI of the property
   * @param value       {string} the literal value
   *
   * @returns {boolean} whether or not there is a source
   */
  _existsLiteralTarget: function HS__existsLiteralTarget(propertyURI, value) {
    var property = this._rdf.GetResource(propertyURI);
    var target = this._rdf.GetLiteral(value);

    return this._ds.hasArcIn(target, property);
  },

  /**
   * Get the source for a property set to a given literal value.
   *
   * @param propertyURI {string} the URI of the property
   * @param value       {string} the literal value
   */
  _getSourceForLiteral: function HS__getSourceForLiteral(propertyURI, value) {
    var property = this._rdf.GetResource(propertyURI);
    var target = this._rdf.GetLiteral(value);

    var source = this._ds.GetSource(property, target, true);
    if (source)
      return source.ValueUTF8;

    return null;
  },

  /**
   * Whether or not there is an RDF source that has the given property set to
   * the given resource target.
   * 
   * @param propertyURI {string} the URI of the property
   * @param targetURI   {string} the URI of the target
   *
   * @returns {boolean} whether or not there is a source
   */
  _existsResourceTarget: function HS__existsResourceTarget(propertyURI,
                                                           targetURI) {
    var property = this._rdf.GetResource(propertyURI);
    var target = this._rdf.GetResource(targetURI);

    return this._ds.hasArcIn(target, property);
  },

  /**
   * Remove a property of an RDF source.
   *
   * @param sourceURI   {string} the URI of the source
   * @param propertyURI {string} the URI of the property
   */
  _removeTarget: function HS__removeTarget(sourceURI, propertyURI) {
    var source = this._rdf.GetResource(sourceURI);
    var property = this._rdf.GetResource(propertyURI);

    if (this._ds.hasArcOut(source, property)) {
      var target = this._ds.GetTarget(source, property, true);
      this._ds.Unassert(source, property, target);
    }
  },

 /**
  * Remove all assertions about a given RDF source.
  *
  * Note: not recursive.  If some assertions point to other resources,
  * and you want to remove assertions about those resources too, you need
  * to do so manually.
  *
  * @param sourceURI {string} the URI of the source
  */
  _removeAssertions: function HS__removeAssertions(sourceURI) {
    var source = this._rdf.GetResource(sourceURI);
    var properties = this._ds.ArcLabelsOut(source);
 
    while (properties.hasMoreElements()) {
      let property = properties.getNext();
      let targets = this._ds.GetTargets(source, property, true);
      while (targets.hasMoreElements()) {
        let target = targets.getNext();
        this._ds.Unassert(source, property, target);
      }
    }
  }

};

//****************************************************************************//
// More XPCOM Plumbing

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([HandlerService]);
