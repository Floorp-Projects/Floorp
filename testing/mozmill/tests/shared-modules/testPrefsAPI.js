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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henrik Skupin <hskupin@mozilla.com>
 *   Clint Talbert <ctalbert@mozilla.com>
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
 * The PrefsAPI adds support for preferences related functions. It gives access
 * to the preferences system and allows to handle the preferences dialog
 *
 * @version 1.0.1
 */

var MODULE_NAME = 'PrefsAPI';

const RELATIVE_ROOT = '.'
const MODULE_REQUIRES = ['ModalDialogAPI'];

const gTimeout = 5000;

// Preferences dialog element templates
const PREF_DIALOG_BUTTONS  = '/{"type":"prefwindow"}/anon({"anonid":"dlg-buttons"})';
const PREF_DIALOG_DECK     = '/{"type":"prefwindow"}/anon({"class":"paneDeckContainer"})/anon({"anonid":"paneDeck"})';
const PREF_DIALOG_SELECTOR = '/{"type":"prefwindow"}/anon({"orient":"vertical"})/anon({"anonid":"selector"})';

/**
 * Constructor
 * 
 * @param {MozMillController} controller
 *        MozMill controller of the browser window to operate on.
 */
function preferencesDialog(controller)
{
  this._controller = controller;
}

/**
 * Preferences dialog object to simplify the access to this dialog
 */
preferencesDialog.prototype = {
  /**
   * Returns the MozMill controller
   *
   * @returns Mozmill controller
   * @type {MozMillController}
   */
  get controller() {
    return this._controller;
  },

  /**
   * Retrieve the currently selected panel
   *
   * @returns The panel element
   * @type {ElemBase}
   */
  get selectedPane() {
    return this.getElement({type: "deck_pane"});
  },

  /**
   * Get the given pane id
   */
  get paneId() {
    // Check if the selector and the pane are consistent
    var selector = this.getElement({type: "selector"});

    this._controller.waitForEval("subject.selector.getAttribute('pane') == subject.dlg.selectedPane.getNode().id", gTimeout, 100,
                                 {selector: selector.getNode().selectedItem, dlg: this});

    return this.selectedPane.getNode().id;
  },

  /**
   * Set the given pane by id
   *
   * @param {string} id of the pane
   */
  set paneId(id) {
    var button = this.getElement({type: "selector_button", value: id});
    this._controller.waitThenClick(button, gTimeout);

    // Check if the correct selector is selected
    var selector = this.getElement({type: "selector"});
    this._controller.waitForEval("subject.selector.getAttribute('pane') == subject.newPane", gTimeout, 100,
                                 {selector: selector.getNode().selectedItem, newPane: id});
    return this.paneId;
  },

  /**
   * Close the preferences dialog
   *
   * @param {MozMillController} controller
   *        MozMillController of the window to operate on
   * @param {boolean} saveChanges
   *        (Optional) If true the OK button is clicked on Windows which saves
   *        the changes. On OS X and Linux changes are applied immediately
   */
  close : function preferencesDialog_close(saveChanges) {
    saveChanges = (saveChanges == undefined) ? false : saveChanges;

    if (mozmill.isWindows) {
      var button = this.getElement({type: "button", subtype: (saveChanges ? "accept" : "cancel")});
      this._controller.click(button);
    } else {
      this._controller.keypress(null, 'VK_ESCAPE', {});
    }
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
  getElement : function aboutSessionRestore_getElement(spec) {
    var elem = null;

    switch(spec.type) {
      case "button":
        elem = new elementslib.Lookup(this._controller.window.document, PREF_DIALOG_BUTTONS +
                                      '/{"dlgtype":"' + spec.subtype + '"}');
        break;
      case "deck":
        elem = new elementslib.Lookup(this._controller.window.document, PREF_DIALOG_DECK);
        break;
      case "deck_pane":
        var deck = this.getElement({type: "deck"}).getNode();

        // XXX: Bug 390724 - selectedPane is broken. So iterate through all elements
        var panel = deck.boxObject.firstChild;
        for (var ii = 0; ii < deck.selectedIndex; ii++)
          panel = panel.nextSibling;

        elem = new elementslib.Elem(panel);
        break;
      case "selector":
        elem = new elementslib.Lookup(this._controller.window.document, PREF_DIALOG_SELECTOR);
        break;
      case "selector_button":
        elem = new elementslib.Lookup(this._controller.window.document, PREF_DIALOG_SELECTOR +
                                      '/{"pane":"' + spec.value + '"}');
        break;
      default:
        throw new Error(arguments.callee.name + ": Unknown element type - " + spec.type);
    }

    return elem;
  }
};

/**
 * Preferences object to simplify the access to the nsIPrefBranch.
 */
var preferences = {
  _branch : Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefBranch),

  /**
   * Use branch to access low level functions of nsIPrefBranch
   *
   * @return Instance of the preferences branch
   * @type nsIPrefBranch
   */
  get branch() {
    return this._branch;
  },

  /**
   * Clear a user set preference
   *
   * @param {string} prefName
   *        The user-set preference to clear
   * @return False if the preference had the default value
   * @type boolean
   **/
  clearUserPref : function preferences_clearUserPref(prefName) {
    try {
      this._branch.clearUserPref(prefName);
      return true;
    } catch (e) {
      return false;
    }
  },

  /**
   * Retrieve the value of an individual preference.
   *
   * @param {string} prefName
   *        The preference to get the value of.
   * @param {boolean/number/string} defaultValue
   *        The default value if preference cannot be found.
   * @return The value of the requested preference
   * @type boolean/int/string
   */
  getPref : function preferences_getPref(prefName, defaultValue) {
    try {
      switch (typeof defaultValue) {
        case ('boolean'):
          return this._branch.getBoolPref(prefName);
        case ('string'):
          return this._branch.getCharPref(prefName);
        case ('number'):
          return this._branch.getIntPref(prefName);
        default:
          return undefined;
      }
    } catch(e) {
      return defaultValue;
    }
  },

  /**
   * Set the value of an individual preference.
   *
   * @param {string} prefName
   *        The preference to set the value of.
   * @param {boolean/number/string} value
   *        The value to set the preference to.
   *
   * @return Returns if the value was successfully set.
   * @type boolean
   */
  setPref : function preferences_setPref(name, value) {
    try {
      switch (typeof value) {
        case ('boolean'):
          this._branch.setBoolPref(name, value);
          break;
        case ('string'):
          this._branch.setCharPref(name, value);
          break;
        case ('number'):
          this._branch.setIntPref(name, value);
          break;
        default:
          return false;
      }
    } catch(e) {
      return false;
    }

    return true;
  }
};

/**
 * Open the preferences dialog and call the given handler
 *
 * @param {function} callback
 *        The callback handler to use to interact with the preference dialog
 * @param {function} launcher
 *        (Optional) A callback handler to launch the preference dialog
 */
function openPreferencesDialog(callback, launcher)
{
  var prefCtrl = null;

  if(!callback)
    throw new Error("No callback given for Preferences Dialog");

  if (mozmill.isWindows) {
    // Preference dialog is modal on windows, set up our callback
    var md = collector.getModule('ModalDialogAPI');
    var prefModal = new md.modalDialog(callback);
    prefModal.start();
  }

  // Launch the preference dialog
  if (launcher) {
    launcher();

    // Now that we've launched the dialog, wait a bit for the window
    mozmill.controller.sleep(500);
    var win = Cc["@mozilla.org/appshell/window-mediator;1"]
                 .getService(Ci.nsIWindowMediator).getMostRecentWindow(null);
    prefCtrl = new mozmill.controller.MozMillController(win);
  } else {
    prefCtrl = new mozmill.getPreferencesController();
  }

  // If the dialog is not modal, run the callback directly
  if (!mozmill.isWindows) {
    prefCtrl.sleep(500);
    callback(prefCtrl);
  }

  // Wait a bit to make sure window has been closed
  mozmill.controller.sleep(500);
}
