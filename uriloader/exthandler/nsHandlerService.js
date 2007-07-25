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


// namespace prefix
const NC_NS                 = "http://home.netscape.com/NC-rdf#";

// type list properties

const NC_MIME_TYPES         = NC_NS + "MIME-types";
const NC_PROTOCOL_SCHEMES   = NC_NS + "Protocol-Schemes";

// content type ("type") properties

const NC_EDITABLE           = NC_NS + "editable";

// nsIMIMEInfo::MIMEType
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

// references nsIHandlerApp record
const NC_PREFERRED_APP      = NC_NS + "externalApplication";

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

  store: function HS_store(aHandlerInfo) {
    // FIXME: when we switch from RDF to something with transactions (like
    // SQLite), enclose the following changes in a transaction so they all
    // get rolled back if any of them fail and we don't leave the datastore
    // in an inconsistent state.

    this._ensureRecordsForType(aHandlerInfo);
    this._storePreferredAction(aHandlerInfo);
    this._storePreferredHandler(aHandlerInfo);
    this._storeAlwaysAsk(aHandlerInfo);
  },


  //**************************************************************************//
  // Storage Methods

  _storePreferredAction: function HS__storePreferredAction(aHandlerInfo) {
    var infoID = this._getInfoID(aHandlerInfo);

    switch(aHandlerInfo.preferredAction) {
      case Ci.nsIHandlerInfo.saveToDisk:
        this._setLiteral(infoID, NC_SAVE_TO_DISK, "true");
        this._removeValue(infoID, NC_HANDLE_INTERNALLY);
        this._removeValue(infoID, NC_USE_SYSTEM_DEFAULT);
        break;

      case Ci.nsIHandlerInfo.handleInternally:
        this._setLiteral(infoID, NC_HANDLE_INTERNALLY, "true");
        this._removeValue(infoID, NC_SAVE_TO_DISK);
        this._removeValue(infoID, NC_USE_SYSTEM_DEFAULT);
        break;

      case Ci.nsIHandlerInfo.useSystemDefault:
        this._setLiteral(infoID, NC_USE_SYSTEM_DEFAULT, "true");
        this._removeValue(infoID, NC_SAVE_TO_DISK);
        this._removeValue(infoID, NC_HANDLE_INTERNALLY);
        break;

      // This value is indicated in the datastore either by the absence of
      // the three properties or by setting them all "false".  Of these two
      // options, the former seems preferable, because it reduces the size
      // of the RDF file and thus the amount of stuff we have to parse.
      case Ci.nsIHandlerInfo.useHelperApp:
      default:
        this._removeValue(infoID, NC_SAVE_TO_DISK);
        this._removeValue(infoID, NC_HANDLE_INTERNALLY);
        this._removeValue(infoID, NC_USE_SYSTEM_DEFAULT);
        break;
    }
  },

  _storePreferredHandler: function HS__storePreferredHandler(aHandlerInfo) {
    var infoID = this._getInfoID(aHandlerInfo);
    var handlerID = this._getPreferredHandlerID(aHandlerInfo);
    var handler = aHandlerInfo.preferredApplicationHandler;

    if (handler) {
      // First add a record for the preferred app to the datasource.  In the
      // process we also need to remove any vestiges of an existing record, so
      // we remove any properties that we aren't overwriting.
      this._setLiteral(handlerID, NC_PRETTY_NAME, handler.name);
      if (handler instanceof Ci.nsILocalHandlerApp) {
        this._setLiteral(handlerID, NC_PATH, handler.executable.path);
        this._removeValue(handlerID, NC_URI_TEMPLATE);
      }
      else {
        handler.QueryInterface(Ci.nsIWebHandlerApp);
        this._setLiteral(handlerID, NC_URI_TEMPLATE, handler.uriTemplate);
        this._removeValue(handlerID, NC_PATH);
      }

      // Finally, make this app be the preferred app for the handler info.
      // Note: at least some code completely ignores this setting and assumes
      // the preferred app is the one whose URI follows the appropriate pattern.
      this._setResource(infoID, NC_PREFERRED_APP, handlerID);
    }
    else {
      // There isn't a preferred handler.  Remove the existing record for it,
      // if any.
      this._removeValue(handlerID, NC_PRETTY_NAME);
      this._removeValue(handlerID, NC_PATH);
      this._removeValue(handlerID, NC_URI_TEMPLATE);
      this._removeValue(infoID, NC_PREFERRED_APP);
    }
  },

  _storeAlwaysAsk: function HS__storeAlwaysAsk(aHandlerInfo) {
    var infoID = this._getInfoID(aHandlerInfo);
    this._setLiteral(infoID,
                     NC_ALWAYS_ASK,
                     aHandlerInfo.alwaysAskBeforeHandling ? "true" : "false");
  },


  //**************************************************************************//
  // Storage Utils

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
      return "mimetype";
    else
      return "scheme";
  },

  /**
   * Return the unique identifier for a content type record, which stores
   * the editable and value fields plus a reference to the type's handler.
   * 
   * |urn:(mimetype|scheme):<type>|
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
   * |urn:(mimetype|scheme):handler:<type>|
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
   * |urn:(mimetype|scheme):externalApplication:<type>|
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
   * Get the list of types for the given class, creating the list if it
   * doesn't already exist.  The class can be either "mimetype" or "scheme"
   * (i.e. the result of a call to _getClass).
   * 
   * |urn:(mimetype|scheme)s|
   * |urn:(mimetype|scheme)s:root|
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
      this._rdf.GetResource(aClass == "mimetype" ? NC_MIME_TYPES
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
    this._setLiteral(typeID, NC_EDITABLE, "true");
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
   * Set a property of an RDF source to a resource.
   *
   * @param sourceURI   {string} the URI of the source
   * @param propertyURI {string} the URI of the property
   * @param resourceURI {string} the URI of the resource
   */
  _setResource: function HS__setResource(sourceURI, propertyURI, resourceURI) {
    var source = this._rdf.GetResource(sourceURI);
    var property = this._rdf.GetResource(propertyURI);
    var target = this._rdf.GetResource(resourceURI);
    
    this._setTarget(source, property, target);
  },

  /**
   * Assert an arc into the RDF datasource if there is no arc with the given
   * source and property; otherwise, if there is already an existing arc,
   * change it to point to the given target.
   *
   * @param source    {nsIRDFResource}  the source
   * @param property  {nsIRDFResource}  the property
   * @param value     {nsIRDFNode}      the target
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
   * Remove a property of an RDF source.
   *
   * @param sourceURI   {string} the URI of the source
   * @param propertyURI {string} the URI of the property
   */
  _removeValue: function HS__removeValue(sourceURI, propertyURI) {
    var source = this._rdf.GetResource(sourceURI);
    var property = this._rdf.GetResource(propertyURI);

    if (this._ds.hasArcOut(source, property)) {
      var target = this._ds.GetTarget(source, property, true);
      this._ds.Unassert(source, property, target, true);
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
