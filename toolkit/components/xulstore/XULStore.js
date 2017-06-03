/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

// Enables logging and shorter save intervals.
const debugMode = false;

// Delay when a change is made to when the file is saved.
// 30 seconds normally, or 3 seconds for testing
const WRITE_DELAY_MS = (debugMode ? 3 : 30) * 1000;

const XULSTORE_CONTRACTID = "@mozilla.org/xul/xulstore;1";
const XULSTORE_CID = Components.ID("{6f46b6f4-c8b1-4bd4-a4fa-9ebbed0753ea}");
const STOREDB_FILENAME = "xulstore.json";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil", "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");

function XULStore() {
  if (!Services.appinfo.inSafeMode)
    this.load();
}

XULStore.prototype = {
  classID: XULSTORE_CID,
  classInfo: XPCOMUtils.generateCI({classID: XULSTORE_CID,
                                    contractID: XULSTORE_CONTRACTID,
                                    classDescription: "XULStore",
                                    interfaces: [Ci.nsIXULStore]}),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsIXULStore,
                                         Ci.nsISupportsWeakReference]),
  _xpcom_factory: XPCOMUtils.generateSingletonFactory(XULStore),

  /* ---------- private members ---------- */

  /*
   * The format of _data is _data[docuri][elementid][attribute]. For example:
   *  {
   *      "chrome://blah/foo.xul" : {
   *                                    "main-window" : { aaa : 1, bbb : "c" },
   *                                    "barColumn"   : { ddd : 9, eee : "f" },
   *                                },
   *
   *      "chrome://foopy/b.xul" :  { ... },
   *      ...
   *  }
   */
  _data: {},
  _storeFile: null,
  _needsSaving: false,
  _saveAllowed: true,
  _writeTimer: Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer),

  load() {
    Services.obs.addObserver(this, "profile-before-change", true);

    this._storeFile = Services.dirsvc.get("ProfD", Ci.nsIFile);
    this._storeFile.append(STOREDB_FILENAME);

    try {
      this.readFile();
    } catch (e) {
      // If readFile() throws, it means that the file didn't exist.
      this.import();
    }
  },

  observe(subject, topic, data) {
    this.writeFile();
    if (topic == "profile-before-change") {
      this._saveAllowed = false;
    }
  },

  /*
   * Internal function for logging debug messages to the Error Console window
   */
  log(message) {
    if (!debugMode)
      return;
    dump("XULStore: " + message + "\n");
    Services.console.logStringMessage("XULStore: " + message);
  },

  import() {
    let localStoreFile = Services.dirsvc.get("ProfD", Ci.nsIFile);

    localStoreFile.append("localstore.rdf");

    const RDF = Cc["@mozilla.org/rdf/rdf-service;1"].getService(Ci.nsIRDFService);
    const persistKey = RDF.GetResource("http://home.netscape.com/NC-rdf#persist");

    this.log("Import localstore from " + localStoreFile.path);

    let localStoreURI = Services.io.newFileURI(localStoreFile).spec;
    let localStore = RDF.GetDataSourceBlocking(localStoreURI);
    let resources = localStore.GetAllResources();

    while (resources.hasMoreElements()) {
      let resource = resources.getNext().QueryInterface(Ci.nsIRDFResource);
      let uri;

      try {
        uri = NetUtil.newURI(resource.ValueUTF8);
      } catch (ex) {
        continue; // skip invalid uris
      }

      // If this has a ref, then this is an attribute reference. Otherwise,
      // this is a document reference.
      if (!uri.hasRef)
          continue;

      // Verify that there the persist key is connected up.
      let docURI = uri.specIgnoringRef;

      if (!localStore.HasAssertion(RDF.GetResource(docURI), persistKey, resource, true))
          continue;

      let id = uri.ref;
      let attrs = localStore.ArcLabelsOut(resource);

      while (attrs.hasMoreElements()) {
        let attr = attrs.getNext().QueryInterface(Ci.nsIRDFResource);
        let value = localStore.GetTarget(resource, attr, true);

        if (value instanceof Ci.nsIRDFLiteral) {
          this.setValue(docURI, id, attr.ValueUTF8, value.Value);
        }
      }
    }
  },

  readFile() {
    const MODE_RDONLY = 0x01;
    const FILE_PERMS  = 0o600;

    let stream = Cc["@mozilla.org/network/file-input-stream;1"].
                 createInstance(Ci.nsIFileInputStream);
    let json = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
    try {
      stream.init(this._storeFile, MODE_RDONLY, FILE_PERMS, 0);
      this._data = json.decodeFromStream(stream, stream.available());
    } catch (e) {
      this.log("Error reading JSON: " + e);
      if (e.result == Cr.NS_ERROR_FILE_NOT_FOUND) {
        throw e; // Let the caller handle it!
      }
    } finally {
      stream.close();
    }
  },

  async writeFile() {
    if (!this._needsSaving)
      return;

    this._needsSaving = false;

    this.log("Writing to xulstore.json");

    try {
      let data = JSON.stringify(this._data);
      let encoder = new TextEncoder();

      data = encoder.encode(data);
      await OS.File.writeAtomic(this._storeFile.path, data,
                              { tmpPath: this._storeFile.path + ".tmp" });
    } catch (e) {
      this.log("Failed to write xulstore.json: " + e);
      throw e;
    }
  },

  markAsChanged() {
    if (this._needsSaving || !this._storeFile)
      return;

    // Don't write the file more than once every 30 seconds.
    this._needsSaving = true;
    this._writeTimer.init(this, WRITE_DELAY_MS, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  /* ---------- interface implementation ---------- */

  setValue(docURI, id, attr, value) {
    this.log("Saving " + attr + "=" + value + " for id=" + id + ", doc=" + docURI);

    if (!this._saveAllowed) {
      Services.console.logStringMessage("XULStore: Changes after profile-before-change are ignored!");
      return;
    }

    // bug 319846 -- don't save really long attributes or values.
    if (id.length > 512 || attr.length > 512) {
      throw Components.Exception("id or attribute name too long", Cr.NS_ERROR_ILLEGAL_VALUE);
    }

    if (value.length > 4096) {
      Services.console.logStringMessage("XULStore: Warning, truncating long attribute value")
      value = value.substr(0, 4096);
    }

    let obj = this._data;
    if (!(docURI in obj)) {
      obj[docURI] = {};
    }
    obj = obj[docURI];
    if (!(id in obj)) {
      obj[id] = {};
    }
    obj = obj[id];

    // Don't set the value if it is already set to avoid saving the file.
    if (attr in obj && obj[attr] == value)
      return;

    obj[attr] = value; // IE, this._data[docURI][id][attr] = value;

    this.markAsChanged();
  },

  hasValue(docURI, id, attr) {
    this.log("has store value for id=" + id + ", attr=" + attr + ", doc=" + docURI);

    let ids = this._data[docURI];
    if (ids) {
      let attrs = ids[id];
      if (attrs) {
        return attr in attrs;
      }
    }

    return false;
  },

  getValue(docURI, id, attr) {
    this.log("get store value for id=" + id + ", attr=" + attr + ", doc=" + docURI);

    let ids = this._data[docURI];
    if (ids) {
      let attrs = ids[id];
      if (attrs) {
        return attrs[attr] || "";
      }
    }

    return "";
  },

  removeValue(docURI, id, attr) {
    this.log("remove store value for id=" + id + ", attr=" + attr + ", doc=" + docURI);

    if (!this._saveAllowed) {
      Services.console.logStringMessage("XULStore: Changes after profile-before-change are ignored!");
      return;
    }

    let ids = this._data[docURI];
    if (ids) {
      let attrs = ids[id];
      if (attrs && attr in attrs) {
        delete attrs[attr];

        if (Object.getOwnPropertyNames(attrs).length == 0) {
          delete ids[id];

          if (Object.getOwnPropertyNames(ids).length == 0) {
            delete this._data[docURI];
          }
        }

        this.markAsChanged();
      }
    }
  },

  getIDsEnumerator(docURI) {
    this.log("Getting ID enumerator for doc=" + docURI);

    if (!(docURI in this._data))
      return new nsStringEnumerator([]);

    let result = [];
    let ids = this._data[docURI];
    if (ids) {
      for (let id in this._data[docURI]) {
        result.push(id);
      }
    }

    return new nsStringEnumerator(result);
  },

  getAttributeEnumerator(docURI, id) {
    this.log("Getting attribute enumerator for id=" + id + ", doc=" + docURI);

    if (!(docURI in this._data) || !(id in this._data[docURI]))
      return new nsStringEnumerator([]);

    let attrs = [];
    for (let attr in this._data[docURI][id]) {
      attrs.push(attr);
    }

    return new nsStringEnumerator(attrs);
  }
};

function nsStringEnumerator(items) {
  this._items = items;
}

nsStringEnumerator.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIStringEnumerator]),
  _nextIndex: 0,
  hasMore() {
    return this._nextIndex < this._items.length;
  },
  getNext() {
    if (!this.hasMore())
      throw Cr.NS_ERROR_NOT_AVAILABLE;
    return this._items[this._nextIndex++];
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([XULStore]);
