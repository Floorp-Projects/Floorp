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
 *   Clint Talbert <ctalbert@mozilla.com>
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
 * The ModalDialogAPI adds support for handling modal dialogs. It
 * has to be used e.g. for alert boxes and other commonDialog instances.
 *
 * @version 1.0.2
 */

/* Huge amounts of code have been leveraged from the password manager mochitest suite.
 * http://mxr.mozilla.org/mozilla-central/source/toolkit/components/passwordmgr/test/prompt_common.js
 */

var frame = {}; Components.utils.import('resource://mozmill/modules/frame.js', frame);

const MODULE_NAME = 'ModalDialogAPI';

/**
 * Observer object to find the modal dialog
 */
var mdObserver = {
  QueryInterface : function (iid) {
    const interfaces = [Ci.nsIObserver,
                        Ci.nsISupports,
                        Ci.nsISupportsWeakReference];

    if (!interfaces.some( function(v) { return iid.equals(v) } ))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  },

  observe : function (subject, topic, data)
  {
    if (this.docFinder()) {
      try {
        var window = mozmill.wm.getMostRecentWindow("");
        this.handler(new mozmill.controller.MozMillController(window));
      } catch (ex) {
          window.close();
          frame.events.fail({'function':ex});
      }
    } else {
      // try again in a bit
      this.startTimer(null, this);
    }
  },

  handler: null,
  startTimer: null,
  docFinder: null
};

/**
 * Create a new modalDialog instance.
 *
 * @class A class to handle modal dialogs
 * @constructor
 * @param {function} callback
 *        The callback handler to use to interact with the modal dialog
 */
function modalDialog(callback)
{
  this.observer = mdObserver;
  this.observer.handler = callback;
  this.observer.startTimer = this.start;
  this.observer.docFinder = this.getDialog;
}

/**
 * Set a new callback handler.
 *
 * @param {function} callback
 *        The callback handler to use to interact with the modal dialog
 */
modalDialog.prototype.setHandler = function modalDialog_setHandler(callback)
{
  this.observer.handler = callback;
}

/**
 * Start timer to wait for the modal dialog.
 *
 * @param {Number} delay
 *        Initial delay before the observer gets called
 * @param {object} observer
 *        (Optional) Observer for modal dialog checks
 */
modalDialog.prototype.start = function modalDialog_start(delay, observer)
{
  const dialogDelay = (delay == undefined) ? 100 : delay;

  var modalDialogTimer = Cc["@mozilla.org/timer;1"].
                         createInstance(Ci.nsITimer);

  // If we are not called from the observer, we have to use the supplied
  // observer instead of this.observer
  if (observer) {
    modalDialogTimer.init(observer,
                          dialogDelay,
                          Ci.nsITimer.TYPE_ONE_SHOT);
  } else {
    modalDialogTimer.init(this.observer,
                          dialogDelay,
                          Ci.nsITimer.TYPE_ONE_SHOT);
  }
}

/**
 * Check if the modal dialog has been opened
 *
 * @private
 * @return Returns if the modal dialog has been found or not
 * @type Boolean
 */
modalDialog.prototype.getDialog = function modalDialog_getDialog()
{
  var enumerator = mozmill.wm.getXULWindowEnumerator("");

  // Find the <browser> which contains notifyWindow, by looking
  // through all the open windows and all the <browsers> in each.
  while (enumerator.hasMoreElements()) {
    var win = enumerator.getNext();
    var windowDocShell = win.QueryInterface(Ci.nsIXULWindow).docShell;

    var containedDocShells = windowDocShell.getDocShellEnumerator(
                                      Ci.nsIDocShellTreeItem.typeChrome,
                                      Ci.nsIDocShell.ENUMERATE_FORWARDS);

    while (containedDocShells.hasMoreElements()) {
      // Get the corresponding document for this docshell
      var childDocShell = containedDocShells.getNext();

      // We don't want it if it's not done loading.
      if (childDocShell.busyFlags != Ci.nsIDocShell.BUSY_FLAGS_NONE)
        continue;

      // Ensure that we are only returning true if it is indeed the modal
      // dialog we were looking for.
      var chrome = win.QueryInterface(Ci.nsIInterfaceRequestor).
                       getInterface(Ci.nsIWebBrowserChrome);
      if (chrome.isWindowModal()) {
        return true;
      }
    }
  }

  return false;
}
