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
 * The Original Code is MozMill Test code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henrik Skupin <hskupin@mozilla.com>
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
 * @fileoverview
 * The AddonsAPI adds support for addons related functions. It also gives
 * access to the Addons Manager.
 */

var MODULE_NAME = 'AddonsAPI';

const RELATIVE_ROOT = '.';
const MODULE_REQUIRES = ['PrefsAPI', 'UtilsAPI'];

const gTimeout = 5000;

// Addons Manager element templates
const AM_TOPBAR       = '/id("extensionsManager")/id("topBar")';
const AM_DECK         = '/id("extensionsManager")/id("addonsMsg")';
const AM_SELECTOR     = AM_TOPBAR + '/{"flex":"1"}/{"class":"viewGroupWrapper"}/id("viewGroup")';
const AM_NOTIFICATION = AM_DECK + '/{"type":"warning"}';
const AM_SEARCHFIELD  = AM_DECK + '/id("extensionsBox")/id("searchPanel")/id("searchfield")';
const AM_SEARCHBUTTON = AM_SEARCHFIELD + '/anon({"class":"textbox-input-box"})/anon({"anonid":"search-icons"})';
const AM_SEARCHINPUT  = AM_SEARCHFIELD + '/anon({"class":"textbox-input-box"})/anon({"anonid":"input"})';
const AM_LISTBOX      = AM_DECK + '/id("extensionsBox")/[1]/id("extensionsView")';
const AM_LISTBOX_BTN  = '/anon({"flex":"1"})/{"class":"addonTextBox"}/{"flex":"1"}';

// Preferences which have to be changed to make sure we do not interact with the
// official AMO page but preview.addons.mozilla.org instead
const AMO_PREFERENCES = [
  {name: "extensions.getAddons.browseAddons", old: "addons.mozilla.org", new: "preview.addons.mozilla.org"},
  {name: "extensions.getAddons.recommended.browseURL", old: "addons.mozilla.org", new: "preview.addons.mozilla.org"},
  {name: "extensions.getAddons.recommended.url", old: "services.addons.mozilla.org", new: "preview.addons.mozilla.org"},
  {name: "extensions.getAddons.search.browseURL", old: "addons.mozilla.org", new: "preview.addons.mozilla.org"},
  {name: "extensions.getAddons.search.url", old: "services.addons.mozilla.org", new: "preview.addons.mozilla.org"},
  {name: "extensions.getMoreThemesURL", old: "addons.mozilla.org", new: "preview.addons.mozilla.org"}
];

/**
 * Constructor
 */
function addonsManager()
{
  this._controller = null;
  this._utilsAPI = collector.getModule('UtilsAPI');
}

/**
 * Addons Manager class
 */
addonsManager.prototype = {
  /**
   * Returns the controller of the window
   *
   * @returns Mozmill Controller
   * @type {MozMillController}
   */
  get controller() {
    return this._controller;
  },

  /**
   * Retrieve the id of the currently selected pane
   *
   * @returns Id of the currently selected pane
   * @type string
   */
  get paneId() {
    var selected = this.getElement({type: "selector_button", subtype: "selected", value: "true"});

    return /\w+/.exec(selected.getNode().id);
  },

  /**
   * Select the pane with the given id
   *
   * @param {string} id
   *        Id of the pane to select
   */
  set paneId(id) {
    if (this.paneId == id)
      return;

    var button = this.getElement({type: "selector_button", subtype: "id", value: id + "-view"});
    this._controller.waitThenClick(button, gTimeout);

    // Wait until the expected pane has been selected
    this._controller.waitForEval("subject.window.paneId == subject.expectedPaneId", gTimeout, 100,
                                 {window: this, expectedPaneId: id});
  },

  /**
   * Clear the search field
   */
  clearSearchField : function addonsManager_clearSearchField() {
    this.paneId = 'search';

    var searchInput = this.getElement({type: "search_fieldInput"});
    var cmdKey = UtilsAPI.getEntity(this.getDtds(), "selectAllCmd.key");
    this._controller.keypress(searchInput, cmdKey, {accelKey: true});
    this._controller.keypress(searchInput, 'VK_DELETE', {});
  },

  /**
   * Close the Addons Manager
   *
   * @param {boolean} force
   *        Force closing of the window
   */
  close : function addonsManager_close(force) {
    var windowCount = mozmill.utils.getWindows().length;

    if (this._controller) {
      // Check if we should force the closing of the AM window
      if (force) {
        this._controller.window.close();
      } else {
        var cmdKey = UtilsAPI.getEntity(this.getDtds(), "closeCmd.key");
        this._controller.keypress(null, cmdKey, {accelKey: true});
      }

      this._controller.waitForEval("subject.getWindows().length == " + (windowCount - 1),
                                   gTimeout, 100, mozmill.utils);
      this._controller = null;
    }
  },

  /**
   * Gets all the needed external DTD urls as an array
   *
   * @returns Array of external DTD urls
   * @type [string]
   */
  getDtds : function downloadManager_getDtds() {
    var dtds = ["chrome://mozapps/locale/extensions/extensions.dtd",
                "chrome://browser/locale/browser.dtd"];
    return dtds;
  },

  /**
   * Retrieve an UI element based on the given spec
   *
   * @param {object} spec
   *        Information of the UI element which should be retrieved
   *        type: General type information
   *        subtype: Specific element or property
   *        value: Value of the element or property
   * @returns Element which has been created
   * @type {ElemBase}
   */
  getElement : function addonsManager_getElement(spec) {
    var elem = null;

    switch(spec.type) {
      case "button_continue":
        elem = new elementslib.ID(this._controller.window.document, "continueDialogButton");
        break;
      case "button_enableAll":
        elem = new elementslib.ID(this._controller.window.document, "enableAllButton");
        break;
      case "button_findUpdates":
        elem = new elementslib.ID(this._controller.window.document, "checkUpdatesAllButton");
        break;
      case "button_hideInformation":
        elem = new elementslib.ID(this._controller.window.document, "hideUpdateInfoButton");
        break;
      case "button_installFile":
        elem = new elementslib.ID(this._controller.window.document, "installFileButton");
        break;
      case "button_installUpdates":
        elem = new elementslib.ID(this._controller.window.document, "installUpdatesAllButton");
        break;
      case "button_showInformation":
        elem = new elementslib.ID(this._controller.window.document, "showUpdateInfoButton");
        break;
      case "button_skip":
        elem = new elementslib.ID(this._controller.window.document, "skipDialogButton");
        break;
      case "link_browseAddons":
        elem = new elementslib.ID(this._controller.window.document, "browseAddons");
        break;
      case "link_getMore":
        elem = new elementslib.ID(this._controller.window.document, "getMore");
        break;
      case "listbox":
        elem = new elementslib.Lookup(this._controller.window.document, AM_LISTBOX);
        break;
      case "listbox_button":
        // The search pane uses a bit differnet markup
        if (this.paneId == "search") {
          elem = new elementslib.Lookup(this._controller.window.document, spec.value.expression +
                                        AM_LISTBOX_BTN + '/{"flex":"1"}/anon({"anonid":"selectedButtons"})' +
                                        '/{"command":"cmd_' + spec.subtype + '"}');
        } else {
          elem = new elementslib.Lookup(this._controller.window.document, spec.value.expression +
                                        AM_LISTBOX_BTN + '/{"command":"cmd_' + spec.subtype + '"}');
        }
        break;
      case "listbox_element":
        elem = new elementslib.Lookup(this._controller.window.document, AM_LISTBOX +
                                      '/{"' + spec.subtype + '":"' + spec.value + '"}');
        break;
      case "notificationBar":
        elem = new elementslib.Lookup(this._controller.window.document, AM_NOTIFICATION);
        break;
      case "notificationBar_buttonRestart":
        elem = new elementslib.Lookup(this._controller.window.document, AM_DECK +
                                      '/{"type":"warning"}');
        break;
      case "search_field":
        elem = new elementslib.Lookup(this._controller.window.document, AM_SEARCHFIELD);
        break;
      case "search_fieldButton":
        elem = new elementslib.Lookup(this._controller.window.document, AM_SEARCHBUTTON);
        break;
      case "search_fieldInput":
        elem = new elementslib.Lookup(this._controller.window.document, AM_SEARCHINPUT);
        break;
      case "search_status":
        elem = new elementslib.ID(this._controller.window.document,
                                  'urn:mozilla:addons:search:status:' + spec.subtype);
        break;
      case "search_statusLabel":
        elem = new elementslib.Elem(spec.value.getNode().boxObject.firstChild);
        break;
      case "selector":
        elem = new elementslib.Lookup(this._controller.window.document, AM_SELECTOR);
        break;
      case "selector_button":
        elem = new elementslib.Lookup(this._controller.window.document, AM_SELECTOR +
                                      '/{"' + spec.subtype + '":"' + spec.value + '"}');
        break;
      default:
        throw new Error(arguments.callee.name + ": Unknown element type - " + spec.type);
    }

    return elem;
  },

  /**
   * Returns the specified item from the richlistbox
   *
   * @param {string} name
   *        nodeName of the wanted richlistitem
   * @param {string} value
   *        nodeValue of the wanted richlistitem
   * @returns Element string of the given list item
   * @type string
   */
  getListboxItem : function addonsManager_getListItem(name, value) {
    return this.getElement({type: "listbox_element", subtype: name, value: value});
  },

  /**
   * Retrieves the pane with the given id
   *
   * @returns The pane
   * @type {ElemBase}
   */
  getPane : function addonsManager_getPane(id) {
    return this.getElement({type: "selector_button", subtype: "id", value: id + "-view"});
  },

  /**
   * Retrieve the current enabled/disabled state of the given plug-in
   *
   * @param {string} node
   *        Node name of the plug-in entry
   * @param {string} value
   *        Node value of the plug-in entry
   * @returns True if plug-in is enabled
   * @type boolean
   */
  getPluginState : function addonsManager_getPluginState(node, value) {
    // If the plugins pane is not selected, do it now
    this.paneId = "plugins";
    
    var plugin = this.getListboxItem(node, value);
    var status = plugin.getNode().getAttribute('isDisabled') == 'false';

    return status;
  },

  /**
   * Open the Addons Manager
   *
   * @param {MozMillController} controller
   *        MozMillController of the window to operate on
   */
  open : function addonsManager_open(controller) {
    controller.click(new elementslib.Elem(controller.menus["tools-menu"].menu_openAddons));
    this.waitForOpened(controller);
  },

  /**
   * Search for the given search term inside the "Get Addons" pane
   *
   * @param {string} searchTerm
   *        Term to search for
   */
  search : function addonsManager_search(searchTerm) {
    // Select the search pane and start search
    this.paneId = "search";

    var searchField = this.getElement({type: "search_field"});

    this.clearSearchField();
    this._controller.waitForElement(searchField, gTimeout);
    this._controller.type(searchField, searchTerm);
    this._controller.keypress(searchField, "VK_RETURN", {});
  },

  /**
   * Set the state of the given plug-in
   *
   * @param {string} node
   *        Node name of the plug-in entry
   * @param {string} value
   *        Node value of the plug-in entry
   * @param {boolean} enable
   *        True if the plug-in should be enabled.
   */
  setPluginState : function addonsManager_setPluginState(node, value, enable) {
    // If plugin already has the target state, do nothing
    if (this.getPluginState(node, value) == enable)
      return;

    // Select the plug-in entry
    var plugin = this.getListboxItem(node, value);
    this._controller.click(plugin);

    // Click the Enable/Disable button
    var subtype = enable ? "enable" : "disable";
    var button = this.getElement({type: "listbox_button", subtype: subtype, value: plugin});

    this._controller.waitThenClick(button, gTimeout);
    this._controller.waitForEval("subject.plugin.getPluginState(subject.node, subject.value) == subject.state", gTimeout, 100,
                                 {plugin: this, node: node, value: value, state: enable});
  },

  /**
   * Waits until the Addons Manager has been opened and returns its controller
   *
   * @param {MozMillController} controller
   *        MozMillController of the window to operate on
   */
  waitForOpened : function addonsManager_waitforOpened(controller) {
    this._controller = this._utilsAPI.handleWindow("type", "Extension:Manager",
                                                   null, true);
  }
};

/**
 *  Updates all necessary preferences to the preview sub domain
 */
function useAmoPreviewUrls() {
  var prefSrv = collector.getModule('PrefsAPI').preferences;

  for each (preference in AMO_PREFERENCES) {
    var pref = prefSrv.getPref(preference.name, "");
    prefSrv.setPref(preference.name, pref.replace(preference.old, preference.new));
  }
}

/**
 * Reset all preferences which point to the preview sub domain
 */
function resetAmoPreviewUrls() {
  var prefSrv = collector.getModule('PrefsAPI').preferences;

  for each (preference in AMO_PREFERENCES) {
    prefSrv.clearUserPref(preference.name);
  }
}
