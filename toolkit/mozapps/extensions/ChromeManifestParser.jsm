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
 * The Original Code is the Extension Manager.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Blair McBride <bmcbride@mozilla.com>
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

"use strict";

var EXPORTED_SYMBOLS = ["ChromeManifestParser"];

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
  Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIChromeFrameMessageManager)
    .sendAsyncMessage(MSG_JAR_FLUSH, aJarFile.path);
}


/**
 * Parses chrome manifest files.
 */
var ChromeManifestParser = {

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
  parseSync: function CMP_parseSync(aURI) {
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
  
  _readFromJar: function CMP_readFromJar(aURI) {
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
  
  _readFromFile: function CMP_readFromFile(aURI) {
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
  hasType: function CMP_hasType(aManifest, aType) {
    return aManifest.some(function(aEntry) {
      return aEntry.type == aType;
    });
  }
};
