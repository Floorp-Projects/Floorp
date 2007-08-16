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

// Inspired by the Places infrastructure in head_bookmarks.js

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

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
    try        { this._dirSvc.get("UMimTyp", Ci.nsIFile) }
    catch (ex) { this._dirSvc.registerProvider(this) }

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
