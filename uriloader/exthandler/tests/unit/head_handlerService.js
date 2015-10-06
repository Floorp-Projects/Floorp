/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Inspired by the Places infrastructure in head_bookmarks.js

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cr = Components.results;
var Cu = Components.utils;

var HandlerServiceTest = {
  //**************************************************************************//
  // Convenience Getters

  __dirSvc: null,
  get _dirSvc() {
    if (!this.__dirSvc)
      this.__dirSvc = Cc["@mozilla.org/file/directory_service;1"].
                      getService(Ci.nsIProperties).
                      QueryInterface(Ci.nsIDirectoryService);
    return this.__dirSvc;
  },

  __consoleSvc: null,
  get _consoleSvc() {
    if (!this.__consoleSvc)
      this.__consoleSvc = Cc["@mozilla.org/consoleservice;1"].
                          getService(Ci.nsIConsoleService);
    return this.__consoleSvc;
  },


  //**************************************************************************//
  // nsISupports
  
  interfaces: [Ci.nsIDirectoryServiceProvider, Ci.nsISupports],

  QueryInterface: function HandlerServiceTest_QueryInterface(iid) {
    if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
      throw Cr.NS_ERROR_NO_INTERFACE;
    return this;
  },


  //**************************************************************************//
  // Initialization & Destruction
  
  init: function HandlerServiceTest_init() {
    // Register ourselves as a directory provider for the datasource file
    // if there isn't one registered already.
    try {
      this._dirSvc.get("UMimTyp", Ci.nsIFile);
    } catch (ex) {
      this._dirSvc.registerProvider(this);
      this._providerRegistered = true;
    }

    // Delete the existing datasource file, if any, so we start from scratch.
    // We also do this after finishing the tests, so there shouldn't be an old
    // file lying around, but just in case we delete it here as well.
    this._deleteDatasourceFile();

    // Turn on logging so we can troubleshoot problems with the tests.
    var prefBranch = Cc["@mozilla.org/preferences-service;1"].
                     getService(Ci.nsIPrefBranch);
    prefBranch.setBoolPref("browser.contentHandling.log", true);
  },

  destroy: function HandlerServiceTest_destroy() {
    // Delete the existing datasource file, if any, so we don't leave test files
    // lying around and we start from scratch the next time.
    this._deleteDatasourceFile();
    // Unregister the directory service provider
    if (this._providerRegistered)
      this._dirSvc.unregisterProvider(this);
  },


  //**************************************************************************//
  // nsIDirectoryServiceProvider

  getFile: function HandlerServiceTest_getFile(property, persistent) {
    this.log("getFile: requesting " + property);

    persistent.value = true;

    if (property == "UMimTyp") {
      var datasourceFile = this._dirSvc.get("CurProcD", Ci.nsIFile);
      datasourceFile.append("mimeTypes.rdf");
      return datasourceFile;
    }

    // This causes extraneous errors to show up in the log when the directory
    // service asks us first for CurProcD and MozBinD.  I wish there was a way
    // to suppress those errors.
    this.log("the following NS_ERROR_FAILURE exception in " +
             "nsIDirectoryServiceProvider::getFile is expected, " +
             "as we don't provide the '" + property + "' file");
    throw Cr.NS_ERROR_FAILURE;
  },


  //**************************************************************************//
  // Utilities

  /**
   * Delete the datasource file.
   */
  _deleteDatasourceFile: function HandlerServiceTest__deleteDatasourceFile() {
    var file = this._dirSvc.get("UMimTyp", Ci.nsIFile);
    if (file.exists())
      file.remove(false);
  },

  /**
   * Get the contents of the datasource as a serialized string.  Useful for
   * debugging problems with test failures, i.e.:
   *
   * HandlerServiceTest.log(HandlerServiceTest.getDatasourceContents());
   *
   * @returns {string} the serialized datasource
   */
  getDatasourceContents: function HandlerServiceTest_getDatasourceContents() {
    var rdf = Cc["@mozilla.org/rdf/rdf-service;1"].getService(Ci.nsIRDFService);

    var ioService = Cc["@mozilla.org/network/io-service;1"].
                    getService(Ci.nsIIOService);
    var fileHandler = ioService.getProtocolHandler("file").
                      QueryInterface(Ci.nsIFileProtocolHandler);
    var fileURL = fileHandler.getURLSpecFromFile(this.getDatasourceFile());
    var ds = rdf.GetDataSourceBlocking(fileURL);

    var outputStream = {
      data: "",
      close: function() {},
      flush: function() {},
      write: function (buffer,count) {
        this.data += buffer;
        return count;
      },
      writeFrom: function (stream,count) {},
      isNonBlocking: false
    };

    ds.QueryInterface(Components.interfaces.nsIRDFXMLSource);
    ds.Serialize(outputStream);

    return outputStream.data;
  },

  /**
   * Log a message to the console and the test log.
   */
  log: function HandlerServiceTest_log(message) {
    message = "*** HandlerServiceTest: " + message;
    this._consoleSvc.logStringMessage(message);
    print(message);
  }

};

HandlerServiceTest.init();
