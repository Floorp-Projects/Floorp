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
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fabrice Desré <fabrice@mozilla.com>
 *   Mark Finkle <mfinkle@mozilla.com>
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

const Cu = Components.utils; 
const Cc = Components.classes;
const Ci = Components.interfaces;

let EXPORTED_SYMBOLS = ["OpenWebapps"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "NetUtil", function() {
  Cu.import("resource://gre/modules/NetUtil.jsm");
  return NetUtil;
});

let OpenWebapps = {
  appsDir: null,
  appsFile: null,
  webapps: { },

  init: function() {
    let file =  Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties).get("ProfD", Ci.nsIFile);
    file.append("webapps");
    if (!file.exists() || !file.isDirectory()) {
      file.create(Ci.nsIFile.DIRECTORY_TYPE, 0700);
    }
    this.appsDir = file;
    this.appsFile = file.clone();
    this.appsFile.append("webapps.json");
    if (!this.appsFile.exists())
      return;
    
    try {
      let channel = NetUtil.newChannel(this.appsFile);
      channel.contentType = "application/json";
      let self = this;
      NetUtil.asyncFetch(channel, function(aStream, aResult) {
        if (!Components.isSuccessCode(aResult)) {
          Cu.reportError("OpenWebappsSupport: Could not read from webapps.json file");
          return;
        }

        // Read webapps json file into a string
        let data = null;
        try {
          data = JSON.parse(NetUtil.readInputStreamToString(aStream, aStream.available()) || "");
          self.webapps = data;
          aStream.close();
        } catch (ex) {
          Cu.reportError("OpenWebsappsStore: Could not parse JSON: " + ex);
        }
      });
    } catch (ex) {
      Cu.reportError("OpenWebappsSupport: Could not read from webapps.json file: " + ex);
    }
  },
 
  _writeFile: function ss_writeFile(aFile, aData) {
    // Initialize the file output stream.
    let ostream = Cc["@mozilla.org/network/safe-file-output-stream;1"].createInstance(Ci.nsIFileOutputStream);
    ostream.init(aFile, 0x02 | 0x08 | 0x20, 0600, ostream.DEFER_OPEN);

    // Obtain a converter to convert our data to a UTF-8 encoded input stream.
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    // Asynchronously copy the data to the file.
    let istream = converter.convertToInputStream(aData);
    NetUtil.asyncCopy(istream, ostream, function(rc) {
      // nothing to do
    });
  },
  
  install: function(aApplication) {
    // Don't install twice an application
    if (this.amInstalled(aApplication.appURI))
      return;

    let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
    let id = uuidGenerator.generateUUID().toString();
    let dir = this.appsDir.clone();
    dir.append(id);
    dir.create(Ci.nsIFile.DIRECTORY_TYPE, 0700);
    
    let manFile = dir.clone();
    manFile.append("manifest.json");
    this._writeFile(manFile, JSON.stringify(aApplication.manifest));
    
    this.webapps[id] = {
      title: aApplication.title,
      storeURI: aApplication.storeURI,
      appURI: aApplication.appURI,
      iconData: aApplication.iconData,
      installData: aApplication.installData,
      installTime: (new Date()).getTime(),
      manifest: aApplication.manifest
    };
    this._writeFile(this.appsFile, JSON.stringify(this.webapps));
  },
 
  amInstalled: function(aURI) {
    for (let id in this.webapps) {
      let app = this.webapps[id];
      if (app.appURI == aURI) {
        return { origin: app.appURI,
                 install_origin: app.storeURI,
                 install_data: app.installData,
                 install_time: app.installTime,
                 manifest: app.manifest };
      }
    }
    return null;
  },

  getInstalledBy: function(aStoreURI) {
    let res = [];
    for (let id in this.webapps) {
      let app = this.webapps[id];
      if (app.storeURI == aStoreURI)
        res.push({ origin: app.appURI,
                   install_origin: app.storeURI,
                   install_data: app.installData,
                   install_time: app.installTime,
                   manifest: app.manifest });
    }
    return res;
  },
  
  mgmtList: function() {
    let res = {};
    for (let id in this.webapps) {
      let app = this.webapps[id];
      res[app.appURI] = { origin: app.appURI,
                          install_origin: app.storeURI,
                          install_data: app.installData,
                          install_time: app.installTime,
                          manifest: app.manifest };
    }
    return res;
  },
  
  mgmtLaunch: function(aOrigin) {
    for (let id in this.webapps) {
      let app = this.webapps[id];
      if (app.appURI == aOrigin) {
        let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
        browserWin.browserDOMWindow.openURI(Services.io.newURI(aOrigin, null, null), null, browserWin.OPEN_APPTAB, Ci.nsIBrowserDOMWindow.OPEN_NEW);
        return true;
      }
    }
    return false;
  },
  
  mgmtUninstall: function(aOrigin) {
    for (let id in this.webapps) {
      let app = this.webapps[id];
      if (app.appURI == aOrigin) {
        delete this.webapps[id];
        this._writeFile(this.appsFile, JSON.stringify(this.webapps));
        let dir = this.appsFile.clone();
        dir.append(id);
        try {
          dir.remove(true);
        } catch (e) {
        }
        return true;
      }
    }
    return false;
  }
};

OpenWebapps.init();
