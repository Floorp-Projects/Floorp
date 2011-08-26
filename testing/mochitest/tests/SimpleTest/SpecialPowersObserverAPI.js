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
 * The Original Code is Special Powers code
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jesse Ruderman <jruderman@mozilla.com>
 *   Robert Sayre <sayrer@gmail.com>
 *   Joel Maher <joel.maher@gmail.com>
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
 * ***** END LICENSE BLOCK *****/

/**
 * Special Powers Exception - used to throw exceptions nicely
 **/
function SpecialPowersException(aMsg) {
  this.message = aMsg;
  this.name = "SpecialPowersException";
}

SpecialPowersException.prototype.toString = function() {
  return this.name + ': "' + this.message + '"';
};

function SpecialPowersObserverAPI() {
  this._crashDumpDir = null;
  this._processCrashObserversRegistered = false;
}

SpecialPowersObserverAPI.prototype = {

  _observe: function(aSubject, aTopic, aData) {
    switch(aTopic) {
      case "plugin-crashed":
      case "ipc:content-shutdown":
        function addDumpIDToMessage(propertyName) {
          var id = aSubject.getPropertyAsAString(propertyName);
          if (id) {
            message.dumpIDs.push(id);
          }
        }

        var message = { type: "crash-observed", dumpIDs: [] };
        aSubject = aSubject.QueryInterface(Components.interfaces.nsIPropertyBag2);
        if (aTopic == "plugin-crashed") {
          addDumpIDToMessage("pluginDumpID");
          addDumpIDToMessage("browserDumpID");
        } else { // ipc:content-shutdown
          addDumpIDToMessage("dumpID");
        }
        this._sendAsyncMessage("SPProcessCrashService", message);
        break;
    }
  },

  _addProcessCrashObservers: function() {
    if (this._processCrashObserversRegistered) {
      return;
    }

    var obs = Components.classes["@mozilla.org/observer-service;1"]
                        .getService(Components.interfaces.nsIObserverService);

    obs.addObserver(this, "plugin-crashed", false);
    obs.addObserver(this, "ipc:content-shutdown", false);
    this._processCrashObserversRegistered = true;
  },

  _removeProcessCrashObservers: function() {
    if (!this._processCrashObserversRegistered) {
      return;
    }

    var obs = Components.classes["@mozilla.org/observer-service;1"]
                        .getService(Components.interfaces.nsIObserverService);

    obs.removeObserver(this, "plugin-crashed");
    obs.removeObserver(this, "ipc:content-shutdown");
    this._processCrashObserversRegistered = false;
  },

  _getCrashDumpDir: function() {
    if (!this._crashDumpDir) {
      var directoryService = Components.classes["@mozilla.org/file/directory_service;1"]
                             .getService(Components.interfaces.nsIProperties);
      this._crashDumpDir = directoryService.get("ProfD", Components.interfaces.nsIFile);
      this._crashDumpDir.append("minidumps");
    }
    return this._crashDumpDir;
  },

  _deleteCrashDumpFiles: function(aFilenames) {
    var crashDumpDir = this._getCrashDumpDir();
    if (!crashDumpDir.exists()) {
      return false;
    }

    var success = aFilenames.length != 0;
    aFilenames.forEach(function(crashFilename) {
      var file = crashDumpDir.clone();
      file.append(crashFilename);
      if (file.exists()) {
        file.remove(false);
      } else {
        success = false;
      }
    });
    return success;
  },

  _findCrashDumpFiles: function(aToIgnore) {
    var crashDumpDir = this._getCrashDumpDir();
    var entries = crashDumpDir.exists() && crashDumpDir.directoryEntries;
    if (!entries) {
      return [];
    }

    var crashDumpFiles = [];
    while (entries.hasMoreElements()) {
      var file = entries.getNext().QueryInterface(Components.interfaces.nsIFile);
      var path = String(file.path);
      if (path.match(/\.(dmp|extra)$/) && !aToIgnore[path]) {
        crashDumpFiles.push(path);
      }
    }
    return crashDumpFiles.concat();
  },

  /**
   * messageManager callback function
   * This will get requests from our API in the window and process them in chrome for it
   **/
  _receiveMessageAPI: function(aMessage) {
    switch(aMessage.name) {
      case "SPPrefService":
        var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                    getService(Components.interfaces.nsIPrefBranch2);
        var prefType = aMessage.json.prefType.toUpperCase();
        var prefName = aMessage.json.prefName;
        var prefValue = "prefValue" in aMessage.json ? aMessage.json.prefValue : null;

        if (aMessage.json.op == "get") {
          if (!prefName || !prefType)
            throw new SpecialPowersException("Invalid parameters for get in SPPrefService");
        } else if (aMessage.json.op == "set") {
          if (!prefName || !prefType  || prefValue === null)
            throw new SpecialPowersException("Invalid parameters for set in SPPrefService");
        } else if (aMessage.json.op == "clear") {
          if (!prefName)
            throw new SpecialPowersException("Invalid parameters for clear in SPPrefService");
        } else {
          throw new SpecialPowersException("Invalid operation for SPPrefService");
        }

        // Now we make the call
        switch(prefType) {
          case "BOOL":
            if (aMessage.json.op == "get")
              return(prefs.getBoolPref(prefName));
            else 
              return(prefs.setBoolPref(prefName, prefValue));
          case "INT":
            if (aMessage.json.op == "get") 
              return(prefs.getIntPref(prefName));
            else
              return(prefs.setIntPref(prefName, prefValue));
          case "CHAR":
            if (aMessage.json.op == "get")
              return(prefs.getCharPref(prefName));
            else
              return(prefs.setCharPref(prefName, prefValue));
          case "COMPLEX":
            if (aMessage.json.op == "get")
              return(prefs.getComplexValue(prefName, prefValue[0]));
            else
              return(prefs.setComplexValue(prefName, prefValue[0], prefValue[1]));
          case "":
            if (aMessage.json.op == "clear") {
              prefs.clearUserPref(prefName);
              return;
            }
        }
        break;

      case "SPProcessCrashService":
        switch (aMessage.json.op) {
          case "register-observer":
            this._addProcessCrashObservers();
            break;
          case "unregister-observer":
            this._removeProcessCrashObservers();
            break;
          case "delete-crash-dump-files":
            return this._deleteCrashDumpFiles(aMessage.json.filenames);
          case "find-crash-dump-files":
            return this._findCrashDumpFiles(aMessage.json.crashDumpFilesToIgnore);
          default:
            throw new SpecialPowersException("Invalid operation for SPProcessCrashService");
        }
        break;

      default:
        throw new SpecialPowersException("Unrecognized Special Powers API");
    }
  }
};

