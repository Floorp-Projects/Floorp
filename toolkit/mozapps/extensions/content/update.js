var gUpdateDialog = {
  _updateType: "",
  _extensionManager: "",
  _extensionID: "",
  _openTime: null,
  _brandShortName: "",
  _updateStrings: null,
  _extensionsToUpdate: [],
  
  _messages: ["update-started", 
              "update-ended", 
              "update-item-started", 
              "update-item-ended",
              "update-item-error"],
  
  init: function ()
  {
    this._updateType = window.arguments[0];
    this._extensionManager = window.arguments[1];
    this._extensionID = window.arguments[2];

    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    for (var i = 0; i < this._messages.length; ++i)
      os.addObserver(this, this._messages[i], false);
    
    this._openTime = Math.abs(Date.UTC());
    
    this._brandShortName = document.getElementById("brandStrings").getString("brandShortName");
    this._updateStrings = document.getElementById("extensionsStrings");

    if (this._updateType == "extensions")
      this._extensionManager.updateExtension(this._extensionID, window);
    else if (gUpdateType == "themes")
      this._extensionManager.updateTheme(this._extensionID);
  },
  
  uninit: function ()
  {
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    for (var i = 0; i < this._messages.length; ++i)
      os.removeObserver(this, this._messages[i]);
  },
  
  cancel: function ()
  {
    // This will cause uninit to be called, removing our listener, so the extension manager's
    // notifications will go nowhere. 
    window.close();
  },
  
  observe: function (aSubject, aTopic, aData)
  {
    switch (aTopic) {
    case "update-started":
      break;
    case "update-item-started":
      break;
    case "update-item-ended":
      this._extensionsToUpdate.push(aSubject);
      break;
    case "update-ended":
      var installObj = { };
      for (var i = 0; i < this._extensionsToUpdate.length; ++i) {  
        var e = this._extensionsToUpdate[i];
        installObj[e.name + " " + e.version] = e.xpiURL;
      }
      if (InstallTrigger.updateEnabled())
        InstallTrigger.install(installObj);

      document.documentElement.acceptDialog();
      break;
/*    
    case "update-start":
      dump("*** update-start: " + aSubject + ", " + aData + "\n");
      break;
    case "update-item-network-start":
      dump("*** update-item-network-start: " + aSubject + ", " + aData + "\n");
      break;
    case "update-item-network-end":
      dump("*** update-item-network-end: " + aSubject + ", " + aData + "\n");
      break;
    case "update-item-processing-start":
      dump("*** update-item-processing-start: " + aSubject + ", " + aData + "\n");
      break;
    case "update-item-processing-end":
      dump("*** update-item-processing-end: " + aSubject + ", " + aData + "\n");
      break;
    case "update-item-error":
      dump("*** update-item-error: " + aSubject + ", " + aData + "\n");
      break;
    case "update-end":
      dump("*** update-end: " + aSubject + ", " + aData + "\n");
      
      // Report Status
      
      // Unhook observers
      var os = Components.classes["@mozilla.org/observer-service;1"]
                         .getService(Components.interfaces.nsIObserverService);
      for (var i = 0; i < this._messages.length; ++i)
        os.removeObserver(this, this._messages[i]);
    
      break;
*/      
    }
  }
};


# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is The Extension Manager.
# 
# The Initial Developer of the Original Code is Ben Goodger.
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Ben Goodger <ben@bengoodger.com>
# 
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK *****
