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
 * The Original Code is Satchel Test Code.
 *
 * The Initial Developer of the Original Code is
 * Ehsan Akhgari.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com> (Original Author)
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
 
netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

/*
 * $_
 *
 * Returns the element with the specified |name| attribute.
 */
function $_(formNum, name) {
  var form = document.getElementById("form" + formNum);
  if (!form) {
    ok(false, "$_ couldn't find requested form " + formNum);
    return null;
  }

  var element = form.elements.namedItem(name);
  if (!element) {
    ok(false, "$_ couldn't find requested element " + name);
    return null;
  }

  // Note that namedItem is a bit stupid, and will prefer an
  // |id| attribute over a |name| attribute when looking for
  // the element.

  if (element.hasAttribute("name") && element.getAttribute("name") != name) {
    ok(false, "$_ got confused.");
    return null;
  }

  return element;
}

// Mochitest gives us a sendKey(), but it's targeted to a specific element.
// This basically sends an untargeted key event, to whatever's focused.
function doKey(aKey, modifier) {
    // Seems we need to enable this again, or sendKeyEvent() complaints.
    netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

    var keyName = "DOM_VK_" + aKey.toUpperCase();
    var key = Components.interfaces.nsIDOMKeyEvent[keyName];

    // undefined --> null
    if (!modifier)
        modifier = null;

    // Window utils for sending fake sey events.
    var wutils = window.QueryInterface(Components.interfaces.nsIInterfaceRequestor).
                          getInterface(Components.interfaces.nsIDOMWindowUtils);

    wutils.sendKeyEvent("keydown",  key, 0, modifier);
    wutils.sendKeyEvent("keypress", key, 0, modifier);
    wutils.sendKeyEvent("keyup",    key, 0, modifier);
}


function getAutocompletePopup() {
    var Ci = Components.interfaces;
    chromeWin = window
                    .QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIWebNavigation)
                    .QueryInterface(Ci.nsIDocShellTreeItem)
                    .rootTreeItem
                    .QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDOMWindow)
                    .QueryInterface(Ci.nsIDOMChromeWindow);
    autocompleteMenu = chromeWin.document.getElementById("PopupAutoComplete");
    ok(autocompleteMenu, "Got autocomplete popup");

    return autocompleteMenu;
}


function cleanUpFormHist() {
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  var formhist = Components.classes["@mozilla.org/satchel/form-history;1"].
                 getService(Components.interfaces.nsIFormHistory2);
  formhist.removeAllEntries();
}
cleanUpFormHist();


var checkObserver = {
  verifyStack: [],
  callback: null,

  waitForChecks: function(callback) {
    if (this.verifyStack.length == 0)
      callback();
    else
      this.callback = callback;
  },

  observe: function(subject, topic, data) {
    netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

    if (data != "addEntry" && data != "modifyEntry")
      return;
    ok(this.verifyStack.length > 0, "checking if saved form data was expected");

    // Make sure that every piece of data we expect to be saved is saved, and no
    // more. Here it is assumed that for every entry satchel saves or modifies, a
    // message is sent.
    //
    // We don't actually check the content of the message, but just that the right
    // quantity of messages is received.
    // - if there are too few messages, test will time out
    // - if there are too many messages, test will error out here
    //
    var expected = this.verifyStack.shift();
    ok(fh.entryExists(expected.name, expected.value), expected.message);

    if (this.verifyStack.length == 0) {
      var callback = this.callback;
      this.callback = null;
      callback();
    }
  }
};

function checkForSave(name, value, message) {
  checkObserver.verifyStack.push({ name : name, value: value, message: message });
}


function getFormSubmitButton(formNum) {
  var form = $("form" + formNum); // by id, not name
  ok(form != null, "getting form " + formNum);

  // we can't just call form.submit(), because that doesn't seem to
  // invoke the form onsubmit handler.
  var button = form.firstChild;
  while (button && button.type != "submit") { button = button.nextSibling; }
  ok(button != null, "getting form submit button");

  return button;
}
