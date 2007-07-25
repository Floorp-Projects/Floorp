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
 * The Original Code is Scriptable IO.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Neil Deakin <enndeakin@sympatico.ca> (Original Author)
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

const SCRIPTABLEIO_CLASS_ID = Components.ID("1EDB3B94-0E2E-419A-8C2B-D9E232D41086");
const SCRIPTABLEIO_CLASS_NAME = "Scriptable IO";
const SCRIPTABLEIO_CONTRACT_ID = "@mozilla.org/io/scriptable-io;1";

const DEFAULT_BUFFER_SIZE = 1024;
const DEFAULT_PERMISSIONS = 0644;

const Cc = Components.classes;
const Ci = Components.interfaces;

var gScriptableIO = null;

// when using the directory service, map some extra keys that are
// easier to understand for common directories
var mappedDirKeys = {
  Application: "resource:app",
  Working: "CurWorkD",
  Profile: "ProfD",
  Desktop: "Desk",
  Components: "ComsD",
  Temp: "TmpD"
};
  
function ScriptableIO() {
}

ScriptableIO.prototype =
{
  _wrapBaseWithInputStream : function ScriptableIO_wrapBaseWithInputStream
                                      (base, modes, permissions)
  {
    var stream;
    if (base instanceof Ci.nsIFile) {
      var behaviour = 0;
      if (modes["deleteonclose"])
        behaviour |= Ci.nsIFileInputStream.DELETE_ON_CLOSE;
      if (modes["closeoneof"])
        behaviour |= Ci.nsIFileInputStream.CLOSE_ON_EOF;
      if (modes["reopenonrewind"])
        behaviour |= Ci.nsIFileInputStream.REOPEN_ON_REWIND;

      stream = Cc["@mozilla.org/network/file-input-stream;1"].
                 createInstance(Ci.nsIFileInputStream);
      stream.init(base, 1, permissions, behaviour);
    }
    else if (base instanceof Ci.nsITransport) {
      var flags = modes["block"] ? Ci.nsITransport.OPEN_BLOCKING : 0;
      stream = base.openInputStream(flags, buffersize, 0);
    }
    else if (base instanceof Ci.nsIInputStream) {
      stream = base;
    }
    else if (typeof base == "string") {
      stream = Cc["@mozilla.org/io/string-input-stream;1"].
                 createInstance(Ci.nsIStringInputStream);
      stream.setData(base, base.length);
    }
    if (!stream)
      throw "Cannot create input stream from base";

    return stream;
  },

  _wrapBaseWithOutputStream : function ScriptableIO_wrapBaseWithOutputStream
                              (base, modes, permissions)
  {
    var stream;
    if (base instanceof Ci.nsIFile) {
      stream = Cc["@mozilla.org/network/file-output-stream;1"].
                 createInstance(Ci.nsIFileOutputStream);

      // default for writing is 'write create truncate'
      var modeFlags = 42;
      if (modes["nocreate"])
        modeFlags ^= 8;
      if (modes["append"])
        modeFlags |= 16;
      if (modes["notruncate"])
        modeFlags ^= 32;
      if (modes["syncsave"])
        modeFlags |= 64;

      stream.init(base, modeFlags, permissions, 0);
    }
    else if (base instanceof Ci.nsITransport) {
      var flags = modes["block"] ? Ci.nsITransport.OPEN_BLOCKING : 0;
      stream = base.openOutputStream(flags, buffersize, 0);
    }
    else if (base instanceof Ci.nsIInputStream) {
      stream = base;
    }
    if (!stream)
      throw "Cannot create output stream from base";

    return stream;
  },

  _openForReading : function ScriptableIO_openForReading
                             (base, modes, buffersize, charset, replchar)
  {
    var stream = base;
    var charstream = null;
    if (modes["text"]) {
      if (!charset)
        charset = "UTF-8";
      if (!replchar)
        replchar = Ci.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER;

      charstream = Cc["@mozilla.org/intl/converter-input-stream;1"].
                     createInstance(Ci.nsIConverterInputStream);
      charstream.init(base, charset, buffersize, replchar);
    }
    else if (modes["buffered"]) {
      stream = Cc["@mozilla.org/network/buffered-input-stream;1"].
                 createInstance(Ci.nsIBufferedInputStream);
      stream.init(base, buffersize);
    }
    else if (modes["multi"]) {
      stream = Cc["@mozilla.org/io/multiplex-input-stream;1"].
                 createInstance(Ci.nsIMultiplexInputStream);
      stream.appendStream(base);
    }

    // wrap the stream in a scriptable stream
    var sstream = Cc["@mozilla.org/scriptableinputstream;1"].createInstance();
    sstream.initWithStreams(stream, charstream);
    return sstream;
  },

  _openForWriting : function ScriptableIO_openForWriting
                             (base, modes, buffersize, charset, replchar)
  {
    var stream = base;
    var charstream = null;
    if (modes["text"]) {
      if (!charset)
        charset = "UTF-8";
      if (!replchar)
        replchar = Ci.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER;

      charstream = Cc["@mozilla.org/intl/converter-output-stream;1"].
                     createInstance(Ci.nsIConverterOutputStream);
      charstream.init(base, charset, buffersize, replchar);
    }
    else if (modes["buffered"]) {
      stream = Cc["@mozilla.org/network/buffered-output-stream;1"].
                 createInstance(Ci.nsIBufferedOutputStream);
      stream.init(base, buffersize);
    }

    // wrap the stream in a scriptable stream
    var sstream = Cc["@mozilla.org/scriptableoutputstream;1"].createInstance();
    sstream.initWithStreams(stream, charstream);
    return sstream;
  },

  getFile : function ScriptableIO_getFile(location, file)
  {
    if (!location)
      throw Components.results.NS_ERROR_INVALID_ARG;

    if (location in mappedDirKeys)
      location = mappedDirKeys[location];

    var ds = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIProperties);
    var fl = ds.get(location, Ci.nsILocalFile);
    if (file)
      fl.append(file);
    return fl;
  },

  getFileWithPath : function ScriptableIO_getFileWithPath(filepath)
  {
    if (!filepath)
      throw Components.results.NS_ERROR_INVALID_ARG;

    // XXXndeakin not sure if setting persistentDescriptor is best here, but
    // it's more useful than initWithPath, for instance, for preferences
    var fl = Cc["@mozilla.org/file/local;1"].createInstance();
    fl.persistentDescriptor = filepath;
    return fl;
  },

  newURI : function ScriptableIO_newURI(uri)
  {
    if (!uri)
      throw Components.results.NS_ERROR_INVALID_ARG;

    var ios = Cc["@mozilla.org/network/io-service;1"].
                getService(Ci.nsIIOService);
    if (uri instanceof Ci.nsIFile)
      return ios.newFileURI(uri);
    return ios.newURI(uri, "", null);
  },

  newInputStream : function ScriptableIO_newInputStream
                            (base, mode, charset, replchar, buffersize)
  {
    return this._newStream(base, mode, charset, replchar, buffersize,
                           DEFAULT_PERMISSIONS, false);
  },

  newOutputStream : function ScriptableIO_newOutputStream
                             (base, mode, charset, replchar, buffersize, permissions)
  {
    return this._newStream(base, mode, charset, replchar, buffersize,
                           permissions, true);
  },

  _newStream : function ScriptableIO_newStream
                        (base, mode, charset, replchar, buffersize, permissions, iswrite)
  {
    if (!base)
      throw Components.results.NS_ERROR_INVALID_ARG;

    var modes = {};
    var modeArr = mode.split(/\s+/);
    for (var m = 0; m < modeArr.length; m++) {
      modes[modeArr[m]] = true;
    }

    if (!buffersize)
      buffersize = DEFAULT_BUFFER_SIZE;

    var stream;
    if (iswrite) {
      base = this._wrapBaseWithOutputStream(base, modes, permissions);
      stream = this._openForWriting(base, modes, buffersize, charset, replchar);
    }
    else {
      base = this._wrapBaseWithInputStream(base, modes, permissions);
      stream = this._openForReading(base, modes, buffersize, charset, replchar);
    }

    if (!stream)
      throw "Cannot create stream from base";

    return stream;
  },

  // nsIClassInfo
  classDescription : "IO",
  classID : SCRIPTABLEIO_CLASS_ID,
  contractID : SCRIPTABLEIO_CONTRACT_ID,
  flags : Ci.nsIClassInfo.SINGLETON,
  implementationLanguage : Ci.nsIProgrammingLanguage.JAVASCRIPT,

  getInterfaces : function ScriptableIO_getInterfaces(aCount) {
    var interfaces = [Ci.nsIScriptableIO, Ci.nsIClassInfo];
    aCount.value = interfaces.length;
    return interfaces;
  },

  getHelperForLanguage : function ScriptableIO_getHelperForLanguage(aCount) {
    return null;
  },
  
  QueryInterface: function ScriptableIO_QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIScriptableIO) ||
        aIID.equals(Ci.nsIClassInfo) ||
        aIID.equals(Ci.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
}

var ScriptableIOFactory = {
  QueryInterface: function ScriptableIOFactory_QueryInterface(iid)
  {
    if (iid.equals(Ci.nsIFactory) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
   
  createInstance: function ScriptableIOFactory_createInstance(aOuter, aIID)
  {
    if (aOuter != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (gScriptableIO == null)
      gScriptableIO = new ScriptableIO();

    return gScriptableIO.QueryInterface(aIID);
  }
};

var ScriptableIOModule = {
  QueryInterface: function ScriptableIOModule_QueryInterface(iid) {
    if (iid.equals(Ci.nsIModule) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  registerSelf: function ScriptableIOModule_registerSelf(aCompMgr, aFileSpec, aLocation, aType)
  {
    aCompMgr = aCompMgr.QueryInterface(Ci.nsIComponentRegistrar);
    aCompMgr.registerFactoryLocation(SCRIPTABLEIO_CLASS_ID,
                                     SCRIPTABLEIO_CLASS_NAME,
                                     SCRIPTABLEIO_CONTRACT_ID,
                                     aFileSpec, aLocation, aType);

    var categoryManager = Cc["@mozilla.org/categorymanager;1"].
                            getService(Ci.nsICategoryManager);

    categoryManager.addCategoryEntry("JavaScript global privileged property", "IO",
                                     SCRIPTABLEIO_CONTRACT_ID, true, true);
  },

  unregisterSelf: function ScriptableIOModule_unregisterSelf(aCompMgr, aLocation, aType)
  {
    aCompMgr = aCompMgr.QueryInterface(Ci.nsIComponentRegistrar);
    aCompMgr.unregisterFactoryLocation(SCRIPTABLEIO_CLASS_ID, aLocation);        

    var categoryManager = Cc["@mozilla.org/categorymanager;1"].
                            getService(Ci.nsICategoryManager);
    categoryManager.deleteCategoryEntry("JavaScript global privileged property",
                                        SCRIPTABLEIO_CONTRACT_ID, true);
  },

  getClassObject: function ScriptableIOModule_getClassObject(aCompMgr, aCID, aIID)
  {
    if (!aIID.equals(Ci.nsIFactory))
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

    if (aCID.equals(SCRIPTABLEIO_CLASS_ID))
      return ScriptableIOFactory;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  canUnload: function ScriptableIOModule_canUnload(aCompMgr)
  {
    gScriptableIO = null;
    return true;
  }
};

function NSGetModule(aCompMgr, aFileSpec) { return ScriptableIOModule; }
