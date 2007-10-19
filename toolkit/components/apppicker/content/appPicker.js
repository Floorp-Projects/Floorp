# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
#
# ***** BEGIN LICENSE BLOCK *****
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Jim Mathies <jmathies@mozilla.com>
#
# Contributor(s):
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

function AppPicker() {};

var g_dialog = null;

AppPicker.prototype = 
{
    // Class members
    _incomingParams:null,

    /** 
    * Init the dialog and populate the application list
    */ 
    appPickerLoad: function appPickerLoad() {
        const nsILocalHandlerApp = Components.interfaces.nsILocalHandlerApp;

        this._incomingParams = window.arguments[0];
        this._incomingParams.handlerApp = null;

        document.title = this._incomingParams.title;

        // Header creation - at the very least, we must have 
        // a mime type:
        //        
        // (icon) Zip File
        // (icon) filename
        //
        // (icon) Web Feed
        // (icon) mime/type
        //
        // (icon) mime/type
        // (icon)

        var mimeInfo = this._incomingParams.mimeInfo;
        var filename = this._incomingParams.filename;
        if (!filename) {
          filename = mimeInfo.MIMEType;
        }
        var description = this._incomingParams.description;
        if (!description) {
          description = filename;
          filename = "";
        }
        
        // Setup the dialog header information
        document.getElementById("content-description").setAttribute("value", 
          description);
        document.getElementById("suggested-filename").setAttribute("value",
          filename);
        document.getElementById("content-icon").setAttribute("src",
          "moz-icon://" + filename + "?size=32&contentType=" +
          mimeInfo.MIMEType);

        // Grab a list of nsILocalHandlerApp application helpers to list
        var fileList = mimeInfo.possibleLocalHandlers;

        /*
          <richlistitem id="app-picker-item" value=nsfileobject>
            <hbox align="center">
              <image id="app-picker-item-image" src=""/>
              <vbox>
                <hbox align="center">
                  <listcell id="app-picker-item-cell" label="Outlook"/>
                </hbox>
              </vbox>
            </hbox>
          </richlistitem>
        */

        var list = document.getElementById("app-picker-list");

        var primaryCount = 0;
        
        if (!fileList || fileList.length == 0) {
          // display a message saying nothing is configured
          document.getElementById("app-picker-notfound").removeAttribute("hidden");
          return;
        }
        
        for (var idx = 0; idx < fileList.length; idx++) {
          var file = fileList.queryElementAt(idx, nsILocalHandlerApp);
          try {
              if (!file.executable || !file.executable.isFile())
                continue;
          } catch (err) {
            continue;
          }

          var item = document.createElement("richlistitem");
          item.setAttribute("id", "app-picker-item");
          item.value = file;
          item.setAttribute("ondblclick", "g_dialog.appDoubleClick();");

          var hbox1 = document.createElement("hbox");
          hbox1.setAttribute("align", "center");

          var image = document.createElement("image");
          image.setAttribute("id", "app-picker-item-image");
          image.setAttribute("src", this.getFileIconURL(file.executable));

          var vbox1 = document.createElement("vbox");
          var hbox2 = document.createElement("hbox");
          hbox2.setAttribute("align", "center");

          var cell = document.createElement("listcell");
          cell.setAttribute("id", "app-picker-item-cell");
          cell.setAttribute("label", this.getFileDisplayName(file.executable));

          hbox2.appendChild(cell);
          vbox1.appendChild(hbox2);
          hbox1.appendChild(image);
          hbox1.appendChild(vbox1);
          item.appendChild(hbox1);
          list.appendChild(item);

          primaryCount++;
        }

        if ( primaryCount == 0 ) {
          // display a message saying nothing is configured
          document.getElementById("app-picker-notfound").removeAttribute("hidden");
        }
    },

    /** 
    * Retrieve the moz-icon for the app
    */ 
    getFileIconURL: function getFileIconURL(file) {
      var ios = Components.classes["@mozilla.org/network/io-service;1"].
                getService(Components.interfaces.nsIIOService);

      if (!ios) return "";
      const nsIFileProtocolHandler =
        Components.interfaces.nsIFileProtocolHandler;

      var fph = ios.getProtocolHandler("file")
                .QueryInterface(nsIFileProtocolHandler);
      if (!fph) return "";

      var urlSpec = fph.getURLSpecFromFile(file);
      return "moz-icon://" + urlSpec + "?size=32";
    },

    /** 
    * Retrieve the pretty description from the file
    */ 
    getFileDisplayName: function getFileDisplayName(file) {
#ifdef XP_WIN
      const nsILocalFileWin = Components.interfaces.nsILocalFileWin;
      if (file instanceof nsILocalFileWin) {
        try {
          return file.getVersionInfoField("FileDescription");
        } catch (e) {
        }
      }
#endif
#ifdef XP_MACOSX
    const nsILocalFileMac = Components.interfaces.nsILocalFileMac;
    if (file instanceof nsILocalFileMac) {
      try {
        return lfm.bundleDisplayName;
      } catch (e) {
      }
    }
#endif
      return file.leafName;
    },

    /**
    * Double click accepts an app
    */
    appDoubleClick: function appDoubleClick() {
      var list = document.getElementById("app-picker-list");
      var selItem = list.selectedItem;

      if (!selItem) {
          this._incomingParams.handlerApp = null;
          return true;
      }

      this._incomingParams.handlerApp = selItem.value;
      window.close();

      return true;
    },

    appPickerOK: function appPickerOK() {
      if (this._incomingParams.handlerApp) return true;

      var list = document.getElementById("app-picker-list");
      var selItem = list.selectedItem;

      if (!selItem) {
        this._incomingParams.handlerApp = null;
        return true;
      }
      this._incomingParams.handlerApp = selItem.value;

      return true;
    },

    appPickerCancel: function appPickerCancel() {
      this._incomingParams.handlerApp = null;
      return true;
    },

    /**
    * User browse for an app.
    */
    appPickerBrowse: function appPickerBrowse() {
      var nsIFilePicker = Components.interfaces.nsIFilePicker;
      var fp = Components.classes["@mozilla.org/filepicker;1"].
               createInstance(nsIFilePicker);

      fp.init(window, this._incomingParams.title, nsIFilePicker.modeOpen);
      fp.appendFilters(nsIFilePicker.filterApps);
      
      var fileLoc = Components.classes["@mozilla.org/file/directory_service;1"]
                            .getService(Components.interfaces.nsIProperties);
      var startLocation;
#ifdef XP_WIN
    startLocation = "ProgF"; // Program Files
#else
#ifdef XP_MACOSX
    startLocation = "LocApp"; // Local Applications
#else
    startLocation = "Home";
#endif
#endif
      fp.displayDirectory = 
        fileLoc.get(startLocation, Components.interfaces.nsILocalFile);
      
      if (fp.show() == nsIFilePicker.returnOK && fp.file) {
          var localHandlerApp = 
            Components.classes["@mozilla.org/uriloader/local-handler-app;1"].
            createInstance(Components.interfaces.nsILocalHandlerApp);
          localHandlerApp.executable = fp.file;

          this._incomingParams.handlerApp = localHandlerApp;
          window.close();
      }
      return true;
    }
}

// Global object
var g_dialog = new AppPicker();
