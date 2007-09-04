/* ***** BEGIN LICENSE BLOCK *****
 *   Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is nsContentDispatchChooser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
 *   Dan Mosedale <dmose@mozilla.org>
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

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

////////////////////////////////////////////////////////////////////////////////
//// Constants

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

const CONTENT_HANDLING_URL = "chrome://mozapps/content/handling/dialog.xul";
// TODO this needs to move to locale, but l10n folks want to wait until we
//      finalize these for sure before doing so
const STRINGBUNDLE_URL = "chrome://mozapps/content/handling/handling.properties";

////////////////////////////////////////////////////////////////////////////////
//// nsContentDispatchChooser class

function nsContentDispatchChooser()
{
}

nsContentDispatchChooser.prototype =
{
  classDescription: "Used to handle different types of content",
  classID: Components.ID("e35d5067-95bc-4029-8432-e8f1e431148d"),
  contractID: "@mozilla.org/content-dispatch-chooser;1",

  //////////////////////////////////////////////////////////////////////////////
  //// nsIContentDispatchChooser

  ask: function ask(aHandler, aWindowContext, aURI, aReason)
  {
    var window = null;
    try {
      if (aWindowContext)
        window = aWindowContext.getInterface(Ci.nsIDOMWindow);
    } catch (e) { /* it's OK to not have a window */ }

    var sbs = Cc["@mozilla.org/intl/stringbundle;1"].
              getService(Ci.nsIStringBundleService);
    var bundle = sbs.createBundle(STRINGBUNDLE_URL);

    var xai = Cc["@mozilla.org/xre/app-info;1"].
              getService(Ci.nsIXULAppInfo);
    // TODO when this is hooked up for content, we will need different strings
    //      for most of these
    var arr = [bundle.GetStringFromName("protocol.title"),
               "",
               bundle.GetStringFromName("protocol.description"),
               bundle.GetStringFromName("protocol.choices.label"),
               bundle.formatStringFromName("protocol.checkbox.label",
                                           [aURI.scheme], 1),
               bundle.formatStringFromName("protocol.checkbox.extra",
                                           [xai.name], 1)];

    var params = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    for each (let text in arr) {
      let string = Cc["@mozilla.org/supports-string;1"].
                   createInstance(Ci.nsISupportsString);
      string.data = text;
      params.appendElement(string, false);
    }
    params.appendElement(aHandler, false);
    params.appendElement(aURI, false);
    params.appendElement(aWindowContext, false);
    
    var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
             getService(Ci.nsIWindowWatcher);
    ww.openWindow(window,
                  CONTENT_HANDLING_URL,
                  null,
                  "chrome,dialog=yes,resizable,centerscreen",
                  params);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentDispatchChooser])
};

////////////////////////////////////////////////////////////////////////////////
//// Module

let components = [nsContentDispatchChooser];

function NSGetModule(compMgr, fileSpec)
{
  return XPCOMUtils.generateModule(components);
}

