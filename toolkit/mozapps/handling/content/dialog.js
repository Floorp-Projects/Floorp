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
 * The Original Code is the Protocol Handler Dialog.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (original author)
 *   Dan Mosedale <dmose@mozilla.org>
 *   Florian Queze <florian@queze.net>
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

/**
 * This dialog builds its content based on arguments passed into it.
 * window.arguments[0]:
 *   The title of the dialog.
 * window.arguments[1]:
 *   The url of the image that appears to the left of the description text
 * window.arguments[2]:
 *   The text of the description that will appear above the choices the user
 *   can choose from.
 * window.arguments[3]:
 *   The text of the label directly above the choices the user can choose from.
 * window.arguments[4]:
 *   This is the text to be placed in the label for the checkbox.  If no text is
 *   passed (ie, it's an empty string), the checkbox will be hidden.
 * window.arguments[5]:
 *   The accesskey for the checkbox
 * window.arguments[6]:
 *   This is the text that is displayed below the checkbox when it is checked.
 * window.arguments[7]:
 *   This is the nsIHandlerInfo that gives us all our precious information.
 * window.arguments[8]:
 *   This is the nsIURI that we are being brought up for in the first place.
 * window.arguments[9]:
 *   The nsIInterfaceRequestor of the parent window; may be null
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

var dialog = {
  //////////////////////////////////////////////////////////////////////////////
  //// Member Variables

  _handlerInfo: null,
  _URI: null,
  _itemChoose: null,
  _okButton: null,
  _windowCtxt: null,
  
  //////////////////////////////////////////////////////////////////////////////
  //// Methods

 /**
  * This function initializes the content of the dialog.
  */
  initialize: function initialize()
  {
    this._handlerInfo = window.arguments[7].QueryInterface(Ci.nsIHandlerInfo);
    this._URI         = window.arguments[8].QueryInterface(Ci.nsIURI);
    this._windowCtxt  = window.arguments[9];
    if (this._windowCtxt)
      this._windowCtxt.QueryInterface(Ci.nsIInterfaceRequestor);
    this._itemChoose  = document.getElementById("item-choose");
    this._okButton    = document.documentElement.getButton("accept");

    this.updateOKButton();

    var description = {
      image: document.getElementById("description-image"),
      text:  document.getElementById("description-text")
    };
    var options = document.getElementById("item-action-text");
    var checkbox = {
      desc: document.getElementById("remember"),
      text:  document.getElementById("remember-text")
    };

    // Setting values
    document.title               = window.arguments[0];
    description.image.src        = window.arguments[1];
    description.text.textContent = window.arguments[2];
    options.value                = window.arguments[3];
    checkbox.desc.label          = window.arguments[4];
    checkbox.desc.accessKey      = window.arguments[5];
    checkbox.text.textContent    = window.arguments[6];

    // Hide stuff that needs to be hidden
    if (!checkbox.desc.label)
      checkbox.desc.hidden = true;

    // UI is ready, lets populate our list
    this.populateList();
  },

 /**
  * Populates the list that a user can choose from.
  */
  populateList: function populateList()
  {
    var items = document.getElementById("items");
    var possibleHandlers = this._handlerInfo.possibleApplicationHandlers;
    var preferredHandler = this._handlerInfo.preferredApplicationHandler;
    var ios = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);
    for (let i = possibleHandlers.length - 1; i >= 0; --i) {
      let app = possibleHandlers.queryElementAt(i, Ci.nsIHandlerApp);
      let elm = document.createElement("richlistitem");
      elm.setAttribute("type", "handler");
      elm.setAttribute("name", app.name);
      elm.obj = app;

      if (app instanceof Ci.nsILocalHandlerApp) {
        // See if we have an nsILocalHandlerApp and set the icon
        let uri = ios.newFileURI(app.executable);
        elm.setAttribute("image", "moz-icon://" + uri.spec + "?size=32");
      }
      else if (app instanceof Ci.nsIWebHandlerApp) {
        let uri = ios.newURI(app.uriTemplate, null, null);
        if (/^https?/.test(uri.scheme)) {
          // Unfortunately we can't use the favicon service to get the favicon,
          // because the service looks for a record with the exact URL we give
          // it, and users won't have such records for URLs they don't visit,
          // and users won't visit the handler's URL template, they'll only
          // visit URLs derived from that template (i.e. with %s in the template
          // replaced by the URL of the content being handled).
          elm.setAttribute("image", uri.prePath + "/favicon.ico");
        }
        elm.setAttribute("description", uri.prePath);
      }
      else if (app instanceof Ci.nsIDBusHandlerApp){
	  elm.setAttribute("description", app.method);  
      }
      else
        throw "unknown handler type";

      items.insertBefore(elm, this._itemChoose);
      if (preferredHandler && app == preferredHandler)
        this.selectedItem = elm;
    }

    if (this._handlerInfo.hasDefaultHandler) {
      let elm = document.createElement("richlistitem");
      elm.setAttribute("type", "handler");
      elm.id = "os-default-handler";
      elm.setAttribute("name", this._handlerInfo.defaultDescription);
    
      items.insertBefore(elm, items.firstChild);
      if (this._handlerInfo.preferredAction == 
          Ci.nsIHandlerInfo.useSystemDefault) 
          this.selectedItem = elm;
    }
    items.ensureSelectedElementIsVisible();
  },
  
 /**
  * Brings up a filepicker and allows a user to choose an application.
  */
  chooseApplication: function chooseApplication()
  {
    var bundle = document.getElementById("base-strings");
    var title = bundle.getString("choose.application.title");

    var fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window, title, Ci.nsIFilePicker.modeOpen);
    fp.appendFilters(Ci.nsIFilePicker.filterApps);

    if (fp.show() == Ci.nsIFilePicker.returnOK && fp.file) {
      let uri = Cc["@mozilla.org/network/util;1"].
                getService(Ci.nsIIOService).
                newFileURI(fp.file);

      let handlerApp = Cc["@mozilla.org/uriloader/local-handler-app;1"].
                       createInstance(Ci.nsILocalHandlerApp);
      handlerApp.executable = fp.file;

      // if this application is already in the list, select it and don't add it again
      let parent = document.getElementById("items");
      for (let i = 0; i < parent.childNodes.length; ++i) {
        let elm = parent.childNodes[i];
        if (elm.obj instanceof Ci.nsILocalHandlerApp && elm.obj.equals(handlerApp)) {
          parent.selectedItem = elm;
          parent.ensureSelectedElementIsVisible();
          return;
        }
      }

      let elm = document.createElement("richlistitem");
      elm.setAttribute("type", "handler");
      elm.setAttribute("name", fp.file.leafName);
      elm.setAttribute("image", "moz-icon://" + uri.spec + "?size=32");
      elm.obj = handlerApp;

      parent.selectedItem = parent.insertBefore(elm, parent.firstChild);
      parent.ensureSelectedElementIsVisible();
    }
  },

 /**
  * Function called when the OK button is pressed.
  */
  onAccept: function onAccept()
  {
    var checkbox = document.getElementById("remember");
    if (!checkbox.hidden) {
      // We need to make sure that the default is properly set now
      if (this.selectedItem.obj) {
        // default OS handler doesn't have this property
        this._handlerInfo.preferredAction = Ci.nsIHandlerInfo.useHelperApp;
        this._handlerInfo.preferredApplicationHandler = this.selectedItem.obj;
      }
      else
        this._handlerInfo.preferredAction = Ci.nsIHandlerInfo.useSystemDefault;
    }
    this._handlerInfo.alwaysAskBeforeHandling = !checkbox.checked;

    var hs = Cc["@mozilla.org/uriloader/handler-service;1"].
             getService(Ci.nsIHandlerService);
    hs.store(this._handlerInfo);

    this._handlerInfo.launchWithURI(this._URI, this._windowCtxt);

    return true;
  },

 /**
  * Determines if the OK button should be disabled or not
  */
  updateOKButton: function updateOKButton()
  {
    this._okButton.disabled = this._itemChoose.selected;
  },

 /**
  * Updates the UI based on the checkbox being checked or not.
  */
  onCheck: function onCheck()
  {
    if (document.getElementById("remember").checked)
      document.getElementById("remember-text").setAttribute("visible", "true");
    else
      document.getElementById("remember-text").removeAttribute("visible");
  },

  /**
   * Function called when the user double clicks on an item of the list
   */
  onDblClick: function onDblClick()
  {
    if (this.selectedItem == this._itemChoose)
      this.chooseApplication();
    else
      document.documentElement.acceptDialog();
  },

  /////////////////////////////////////////////////////////////////////////////
  //// Getters / Setters

 /**
  * Returns/sets the selected element in the richlistbox
  */
  get selectedItem()
  {
    return document.getElementById("items").selectedItem;
  },
  set selectedItem(aItem)
  {
    return document.getElementById("items").selectedItem = aItem;
  }
  
};
