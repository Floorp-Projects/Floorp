//
// window.arguments[0] is the nsIExtensionItemUpdater object
//
// window.arguments[1...] is an array of nsIExtensionItem implementing objects 
// that are to be updated. 
//  * if the array is empty, all items are updated (like a background update
//    check)
//  * if the array contains one or two ExtensionItems, with null id fields, 
//    all items of that /type/ are updated.
//
// This UI can be opened from the following places, and in the following modes:
//
// - from the Version Compatibility Checker at startup
//    as the application starts a check is done to see if the application being
//    started is the same version that was started last time. If it isn't, a
//    list of ExtensionItems that are incompatible with the verison being 
//    started is generated and this UI opened with that list. This UI then
//    offers to check for updates to those ExtensionItems that are compatible
//    with the version of the application being started. 
//    
//    In this mode, the wizard is opened to panel 'mismatch'. 
//
// - from the Extension Manager or Options Dialog or any UI where the user
//   directly requests to check for updates.
//    in this case the user selects ExtensionItem(s) to update and this list
//    is passed to this UI. If a single item is sent with the id field set to
//    null but the type set correctly, this UI will check for updates to all 
//    items of that type.
//
//    In this mode, the wizard is opened to panel 'checking'.
//
// - from the Updates Available Status Bar notification.
//    in this case the background update checking has determined there are new
//    updates that can be installed and has prepared a list for the user to see.
//    They can update by clicking on a status bar icon which passes the list
//    to this UI which lets them choose to install updates. 
//
//    In this mode, the wizard is opened to panel 'updatesFound' if the data
//    set is immediately available, or 'checking' if the user closed the browser
//    since the last background check was performed and the check needs to be
//    performed again. 
//

const nsIExtensionItem = Components.interfaces.nsIExtensionItem;
var gExtensionItems = [];
var gUpdater = null;

var gUpdateWizard = {
  _items: [],

  init: function ()
  {
    gUpdater = window.arguments[0];
    for (var i = 1; i < window.argments.length; ++i)
      this._items.push(window.arguments[i].QueryInterface(Components.interfaces.nsIExtensionItem));

    gMismatchPage.init();
  },
  
  uninit: function ()
  {
    gUpdatePage.uninit();
  }
};


var gMismatchPage = {
  init: function ()
  {
    var incompatible = document.getElementById("mismatch.incompatible");
    
    for (var i = 0; i < gUpdateWizard._items.length; ++i) {
      var item = gUpdateWizard._items[i];
      var listitem = document.createElement("listitem");
      listitem.setAttribute("label", item.name + " " + item.version);
      incompatible.appendChild(listitem);
    }
    
    var strings = document.getElementById("extensionsStrings");
    var next = document.documentElement.getButton("next");
    next.label = strings.getString("mismatchCheckNow");
    var cancel = document.documentElement.getButton("cancel");
    cancel.label = strings.getString("mismatchDontCheck");
  },
  
  onPageAdvanced: function ()
  {
    dump("*** mismatch page advanced\n");
  }
};

var gUpdatePage = {
  _itemsToUpdate: [],
  _messages: ["update-started", 
              "update-ended", 
              "update-item-started", 
              "update-item-ended",
              "update-item-error"],
  
  onPageShow: function ()
  {
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    for (var i = 0; i < this._messages.length; ++i)
      os.addObserver(this, this._messages[i], false);

    var extensions = [];
    var themes = [];
    for (i = 0; i < gExtensionItems.length; ++i) {
      switch (gExtensionItems[i].type) {
      case nsIExtensionItem.TYPE_EXTENSION:
        extensions.push(gExtensionItems[i]);
        break;
      case nsIExtensionItem.TYPE_THEME:
        themes.push(gExtensionItems[i]);
        break;
      }
    }
        
    var em = Components.classes["@mozilla.org/extensions/manager;1"]
                       .getService(Components.interfaces.nsIExtensionManager);
    if (this._updateType == "extensions")
      em.updateExtensions(extensions);
    else if (gUpdateType == "themes")
      em.updateTheme(this._extensionID);
  
  },
  
  uninit: function ()
  {
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    for (var i = 0; i < this._messages.length; ++i)
      os.removeObserver(this, this._messages[i]);
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
# The Original Code is The Update Service.
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
