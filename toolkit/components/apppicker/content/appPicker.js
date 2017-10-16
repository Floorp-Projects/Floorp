/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/AppConstants.jsm");

function AppPicker() {}

AppPicker.prototype =
{
    // Class members
    _incomingParams: null,

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

        var list = document.getElementById("app-picker-listbox");

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

          var item = document.createElement("listitem");
          item.className = "listitem-iconic";
          item.handlerApp = file;
          item.setAttribute("label", this.getFileDisplayName(file.executable));
          item.setAttribute("image", this.getFileIconURL(file.executable));
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
      if (AppConstants.platform == "win") {
        if (file instanceof Components.interfaces.nsILocalFileWin) {
          try {
            return file.getVersionInfoField("FileDescription");
          } catch (e) {}
        }
      } else if (AppConstants.platform == "macosx") {
        if (file instanceof Components.interfaces.nsILocalFileMac) {
          try {
            return file.bundleDisplayName;
          } catch (e) {}
        }
      }
      return file.leafName;
    },

    /**
    * Double click accepts an app
    */
    appDoubleClick: function appDoubleClick() {
      var list = document.getElementById("app-picker-listbox");
      var selItem = list.selectedItem;

      if (!selItem) {
          this._incomingParams.handlerApp = null;
          return true;
      }

      this._incomingParams.handlerApp = selItem.handlerApp;
      window.close();

      return true;
    },

    appPickerOK: function appPickerOK() {
      if (this._incomingParams.handlerApp) return true;

      var list = document.getElementById("app-picker-listbox");
      var selItem = list.selectedItem;

      if (!selItem) {
        this._incomingParams.handlerApp = null;
        return true;
      }
      this._incomingParams.handlerApp = selItem.handlerApp;

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
      if (AppConstants.platform == "win") {
        startLocation = "ProgF"; // Program Files
      } else if (AppConstants.platform == "macosx") {
        startLocation = "LocApp"; // Local Applications
      } else {
        startLocation = "Home";
      }
      fp.displayDirectory =
        fileLoc.get(startLocation, Components.interfaces.nsIFile);

      fp.open(rv => {
          if (rv == nsIFilePicker.returnOK && fp.file) {
              var localHandlerApp =
                Components.classes["@mozilla.org/uriloader/local-handler-app;1"].
                createInstance(Components.interfaces.nsILocalHandlerApp);
              localHandlerApp.executable = fp.file;

              this._incomingParams.handlerApp = localHandlerApp;
              window.close();
          }
      });
      return true;
    }
};

// Global object
var g_dialog = new AppPicker();
