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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Myk Melez <myk@mozilla.org>
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

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;


const CLASS_MIMEINFO        = "mimetype";
const CLASS_PROTOCOLINFO    = "scheme";


// namespace prefix
const NC_NS                 = "http://home.netscape.com/NC-rdf#";

// type list properties

const NC_MIME_TYPES         = NC_NS + "MIME-types";
const NC_PROTOCOL_SCHEMES   = NC_NS + "Protocol-Schemes";

// content type ("type") properties

// nsIHandlerInfo::type
const NC_VALUE              = NC_NS + "value";

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


Cu.import("resource://gre/modules/XPCOMUtils.jsm");


function HandlerService() {}

HandlerService.prototype = {
  //**************************************************************************//
  // XPCOM Plumbing

  classDescription: "Handler Service",
  classID:          Components.ID("{32314cc8-22f7-4f7f-a645-1a45453ba6a6}"),
  contractID:       "@mozilla.org/uriloader/handler-service;1",
  QueryInterface:   XPCOMUtils.generateQI([Ci.nsIHandlerService]),


  //**************************************************************************//
  // nsIHandlerService

  enumerate: function HS_enumerate() {
    var handlers = Cc["@mozilla.org/array;1"].
                   createInstance(Ci.nsIMutableArray);
    this._appendHandlers(handlers, CLASS_MIMEINFO);
    this._appendHandlers(handlers, CLASS_PROTOCOLINFO);
    return handlers.enumerate();
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

    // Write the changes to the database immediately so we don't lose them
    // if the application crashes.
    if (this._ds instanceof Ci.nsIRDFRemoteDataSource)
      this._ds.Flush();
  },

  remove: function HS_remove(aHandlerInfo) {
    var preferredHandlerID = this._getPreferredHandlerID(aHandlerInfo);
    this._removeAssertions(preferredHandlerID);

    var infoID = this._getInfoID(aHandlerInfo);
    this._removeAssertions(infoID);

    var typeID = this._getTypeID(aHandlerInfo);
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


  //**************************************************************************//
  // Storage Methods

  _storePreferredAction: function HS__storePreferredAction(aHandlerInfo) {
    var infoID = this._getInfoID(aHandlerInfo);

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

  _storePreferredHandler: function HS__storePreferredHandler(aHandlerInfo) {
    var infoID = this._getInfoID(aHandlerInfo);
    var handlerID = this._getPreferredHandlerID(aHandlerInfo);
    var handler = aHandlerInfo.preferredApplicationHandler;

    if (handler) {
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
    var infoID = this._getInfoID(aHandlerInfo);

    // First, retrieve the set of handler apps currently stored for the type,
    // keeping track of their IDs in a hash that we'll use to determine which
    // ones are no longer valid and should be removed.
    var currentHandlerApps = {};
    var currentHandlerTargets = this._getTargets(infoID, NC_POSSIBLE_APP);
    while (currentHandlerTargets.hasMoreElements()) {
      let handlerApp = currentHandlerTargets.getNext();
      if (handlerApp instanceof Ci.nsIRDFResource) {
        let handlerAppID = handlerApp.Value;
        currentHandlerApps[handlerAppID] = true;
      }
    }

    // Next, store any new handler apps.
    var newHandlerApps =
      aHandlerInfo.possibleApplicationHandlers.enumerate();
    while (newHandlerApps.hasMoreElements()) {
      let handlerApp =
        newHandlerApps.getNext().QueryInterface(Ci.nsIHandlerApp);
      let handlerAppID = this._getPossibleHandlerAppID(handlerApp);
      if (!this._hasResourceTarget(infoID, NC_POSSIBLE_APP, handlerAppID)) {
        this._storeHandlerApp(handlerAppID, handlerApp);
        this._addResourceTarget(infoID, NC_POSSIBLE_APP, handlerAppID);
      }
      delete currentHandlerApps[handlerAppID];
    }

    // Finally, remove any old handler apps that aren't being used anymore,
    // and if those handler apps aren't being used by any other type either,
    // then completely remove their record from the datastore so we don't
    // leave it clogged up with information about handler apps we don't care
    // about anymore.
    for (let handlerAppID in currentHandlerApps) {
      this._removeTarget(infoID, NC_POSSIBLE_APP, handlerAppID);
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
    }
    else {
      aHandlerApp.QueryInterface(Ci.nsIWebHandlerApp);
      this._setLiteral(aHandlerAppID, NC_URI_TEMPLATE, aHandlerApp.uriTemplate);
      this._removeTarget(aHandlerAppID, NC_PATH);
    }
  },

  _storeAlwaysAsk: function HS__storeAlwaysAsk(aHandlerInfo) {
    var infoID = this._getInfoID(aHandlerInfo);
    this._setLiteral(infoID,
                     NC_ALWAYS_ASK,
                     aHandlerInfo.alwaysAskBeforeHandling ? "true" : "false");
  },


  //**************************************************************************//
  // Convenience Getters

  // MIME Service
  __mimeSvc: null,
  get _mimeSvc() {
    if (!this.__mimeSvc)
      this.__mimeSvc =
        Cc["@mozilla.org/uriloader/external-helper-app-service;1"].
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
      var fileLocator = Cc["@mozilla.org/file/directory_service;1"].
                        getService(Ci.nsIProperties);
      var file = fileLocator.get("UMimTyp", Ci.nsIFile);
      // FIXME: make this a memoizing getter if we use it anywhere else.
      var ioService = Cc["@mozilla.org/network/io-service;1"].
                      getService(Ci.nsIIOService);
      var fileHandler = ioService.getProtocolHandler("file").
                        QueryInterface(Ci.nsIFileProtocolHandler);
      this.__ds =
        this._rdf.GetDataSourceBlocking(fileHandler.getURLSpecFromFile(file));
    }

    return this.__ds;
  },


  //**************************************************************************//
  // Storage Utils

  /**
   * Get the string identifying whether this is a MIME or a protocol handler.
   * This string is used in the URI IDs of various RDF properties.
   * 
   * @param aHandlerInfo {nsIHandlerInfo} the handler for which to get the class
   * 
   * @returns {string} the ID
   */
  _getClass: function HS__getClass(aHandlerInfo) {
    if (aHandlerInfo instanceof Ci.nsIMIMEInfo)
      return CLASS_MIMEINFO;
    else
      return CLASS_PROTOCOLINFO;
  },

  /**
   * Return the unique identifier for a content type record, which stores
   * the value field plus a reference to the type's handler.
   * 
   * |urn:<class>:<type>|
   * 
   * XXX: should this be a property of nsIHandlerInfo?
   * 
   * @param aHandlerInfo {nsIHandlerInfo} the type for which to get the ID
   * 
   * @returns {string} the ID
   */
  _getTypeID: function HS__getTypeID(aHandlerInfo) {
    return "urn:" + this._getClass(aHandlerInfo) + ":" + aHandlerInfo.type;
  },

  /**
   * Return the unique identifier for a type info record, which stores
   * the preferredAction and alwaysAsk fields plus a reference to the preferred
   * handler.  Roughly equivalent to the nsIHandlerInfo interface.
   * 
   * |urn:<class>:handler:<type>|
   * 
   * FIXME: the type info record should be merged into the type record,
   * since there's a one to one relationship between them, and this record
   * merely stores additional attributes of a content type.
   * 
   * @param aHandlerInfo {nsIHandlerInfo} the handler for which to get the ID
   * 
   * @returns {string} the ID
   */
  _getInfoID: function HS__getInfoID(aHandlerInfo) {
    return "urn:" + this._getClass(aHandlerInfo) + ":handler:" +
           aHandlerInfo.type;
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
   * @param aHandlerInfo {nsIHandlerInfo} the handler for which to get the ID
   * 
   * @returns {string} the ID
   */
  _getPreferredHandlerID: function HS__getPreferredHandlerID(aHandlerInfo) {
    return "urn:" + this._getClass(aHandlerInfo) + ":externalApplication:" +
           aHandlerInfo.type;
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
    else {
      aHandlerApp.QueryInterface(Ci.nsIWebHandlerApp);
      handlerAppID += "web:" + aHandlerApp.uriTemplate;
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
    // FIXME: once nsIHandlerInfo supports retrieving the scheme
    // (and differentiating between MIME and protocol content types),
    // implement support for protocols.

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
    var typeID = this._getTypeID(aHandlerInfo);
    var type = this._rdf.GetResource(typeID);
    if (typeList.IndexOf(type) != -1)
      return;

    // Create a basic type record for this type.
    typeList.AppendElement(type);
    this._setLiteral(typeID, NC_VALUE, aHandlerInfo.type);
    
    // Create a basic info record for this type.
    var infoID = this._getInfoID(aHandlerInfo);
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
    var preferredHandlerID = this._getPreferredHandlerID(aHandlerInfo);
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
      if (typeList.Resource.Value == "urn:mimetypes:root")
        handler = this._mimeSvc.getFromTypeAndExtension(type, null);
      else
        handler = this._protocolSvc.getProtocolHandlerInfo(type);

      aHandlers.appendElement(handler, false);
    }
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
  _addResourceTarget: function HS__addResourceTarget(sourceURI, propertyURI,
                                                     targetURI) {
    var source = this._rdf.GetResource(sourceURI);
    var property = this._rdf.GetResource(propertyURI);
    var target = this._rdf.GetResource(targetURI);
    
    this._ds.Assert(source, property, target, true);
  },

  /**
   * Whether or not a property of an RDF source has a given resource target.
   * 
   * The difference between this method and _setResource is that this one adds
   * an assertion even if one already exists, which allows its callers to make
   * sets of assertions (i.e. to set a property to multiple targets).
   *
   * @param sourceURI   {string} the URI of the source
   * @param propertyURI {string} the URI of the property
   * @param targetURI   {string} the URI of the target
   *
   * @returns {boolean} whether or not there is such an assertion
   */
  _hasResourceTarget: function HS__hasResourceTarget(sourceURI, propertyURI,
                                                     targetURI) {
    var source = this._rdf.GetResource(sourceURI);
    var property = this._rdf.GetResource(propertyURI);
    var target = this._rdf.GetResource(targetURI);

    return this._ds.HasAssertion(source, property, target, true);
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
      var property = properties.getNext();
      var target = this._ds.GetTarget(source, property, true);
      this._ds.Unassert(source, property, target);
    }
  },


  //**************************************************************************//
  // Utilities

  // FIXME: given that I keep copying them from JS component to JS component,
  // these utilities should all be in a JavaScript module or FUEL interface.

  /**
   * Get an app pref or a default value if the pref doesn't exist.
   *
   * @param   aPrefName
   * @param   aDefaultValue
   * @returns the pref's value or the default (if it is missing)
   */
  _getAppPref: function _getAppPref(aPrefName, aDefaultValue) {
    try {
      var prefBranch = Cc["@mozilla.org/preferences-service;1"].
                       getService(Ci.nsIPrefBranch);
      switch (prefBranch.getPrefType(aPrefName)) {
        case prefBranch.PREF_STRING:
          return prefBranch.getCharPref(aPrefName);

        case prefBranch.PREF_INT:
          return prefBranch.getIntPref(aPrefName);

        case prefBranch.PREF_BOOL:
          return prefBranch.getBoolPref(aPrefName);
      }
    }
    catch (ex) { /* return the default value */ }
    
    return aDefaultValue;
  },

  // Console Service
  __consoleSvc: null,
  get _consoleSvc() {
    if (!this.__consoleSvc)
      this.__consoleSvc = Cc["@mozilla.org/consoleservice;1"].
                          getService(Ci.nsIConsoleService);
    return this.__consoleSvc;
  },

  _log: function _log(aMessage) {
    if (!this._getAppPref("browser.contentHandling.log", false))
      return;

    aMessage = "*** HandlerService: " + aMessage;
    dump(aMessage + "\n");
    this._consoleSvc.logStringMessage(aMessage);
  }
};


//****************************************************************************//
// More XPCOM Plumbing

function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule([HandlerService]);
}
