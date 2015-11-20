/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ChromeManifestParser"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

const MSG_JAR_FLUSH = "AddonJarFlush";


/**
 * Sends local and remote notifications to flush a JAR file cache entry
 *
 * @param aJarFile
 *        The ZIP/XPI/JAR file as a nsIFile
 */
function flushJarCache(aJarFile) {
  Services.obs.notifyObservers(aJarFile, "flush-cache-entry", null);
  Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIMessageBroadcaster)
    .broadcastAsyncMessage(MSG_JAR_FLUSH, aJarFile.path);
}


/**
 * Parses chrome manifest files.
 */
this.ChromeManifestParser = {

  /**
   * Reads and parses a chrome manifest file located at a specified URI, and all
   * secondary manifests it references.
   *
   * @param  aURI
   *         A nsIURI pointing to a chrome manifest.
   *         Typically a file: or jar: URI.
   * @return Array of objects describing each manifest instruction, in the form:
   *         { type: instruction-type, baseURI: string-uri, args: [arguments] }
   **/
  parseSync: function(aURI) {
    function parseLine(aLine) {
      let line = aLine.trim();
      if (line.length == 0 || line.charAt(0) == '#')
        return;
      let tokens = line.split(/\s+/);
      let type = tokens.shift();
      if (type == "manifest") {
        let uri = NetUtil.newURI(tokens.shift(), null, aURI);
        data = data.concat(this.parseSync(uri));
      } else {
        data.push({type: type, baseURI: baseURI, args: tokens});
      }
    }

    let contents = "";
    try {
      if (aURI.scheme == "jar")
        contents = this._readFromJar(aURI);
      else
        contents = this._readFromFile(aURI);
    } catch (e) {
      // Silently fail.
    }

    if (!contents)
      return [];

    let baseURI = NetUtil.newURI(".", null, aURI).spec;

    let data = [];
    let lines = contents.split("\n");
    lines.forEach(parseLine.bind(this));
    return data;
  },
  
  _readFromJar: function(aURI) {
    let data = "";
    let entries = [];
    let readers = [];
    
    try {
      // Deconstrict URI, which can be nested jar: URIs. 
      let uri = aURI.clone();
      while (uri instanceof Ci.nsIJARURI) {
        entries.push(uri.JAREntry);
        uri = uri.JARFile;
      }

      // Open the base jar.
      let reader = Cc["@mozilla.org/libjar/zip-reader;1"].
                   createInstance(Ci.nsIZipReader);
      reader.open(uri.QueryInterface(Ci.nsIFileURL).file);
      readers.push(reader);
  
      // Open the nested jars.
      for (let i = entries.length - 1; i > 0; i--) {
        let innerReader = Cc["@mozilla.org/libjar/zip-reader;1"].
                          createInstance(Ci.nsIZipReader);
        innerReader.openInner(reader, entries[i]);
        readers.push(innerReader);
        reader = innerReader;
      }
      
      // First entry is the actual file we want to read.
      let zis = reader.getInputStream(entries[0]);
      data = NetUtil.readInputStreamToString(zis, zis.available());
    }
    finally {
      // Close readers in reverse order.
      for (let i = readers.length - 1; i >= 0; i--) {
        readers[i].close();
        flushJarCache(readers[i].file);
      }
    }
    
    return data;
  },
  
  _readFromFile: function(aURI) {
    let file = aURI.QueryInterface(Ci.nsIFileURL).file;
    if (!file.exists() || !file.isFile())
      return "";
    
    let data = "";
    let fis = Cc["@mozilla.org/network/file-input-stream;1"].
              createInstance(Ci.nsIFileInputStream);
    try {
      fis.init(file, -1, -1, false);
      data = NetUtil.readInputStreamToString(fis, fis.available());
    } finally {
      fis.close();
    }
    return data;
  },

  /**
  * Detects if there were any instructions of a specified type in a given
  * chrome manifest.
  *
  * @param  aManifest
  *         Manifest data, as returned by ChromeManifestParser.parseSync().
  * @param  aType
  *         Instruction type to filter by.
  * @return True if any matching instructions were found in the manifest.
  */
  hasType: function(aManifest, aType) {
    return aManifest.some(function(aEntry) {
      return aEntry.type == aType;
    });
  }
};
