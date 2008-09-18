/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab filetype=javascript
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

var EXPORTED_SYMBOLS = [ "PlacesBackground" ];

////////////////////////////////////////////////////////////////////////////////
//// Constants

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

const kQuitApplication = "quit-application";
const kPlacesBackgroundShutdown = "places-background-shutdown";

////////////////////////////////////////////////////////////////////////////////
//// nsPlacesBackgound class

function nsPlacesBackground()
{
  let tm = Cc["@mozilla.org/thread-manager;1"].
           getService(Ci.nsIThreadManager);
  this._thread = tm.newThread(0);

  let os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  os.addObserver(this, kQuitApplication, false);
}

nsPlacesBackground.prototype = {
  //////////////////////////////////////////////////////////////////////////////
  //// nsIEventTarget

  dispatch: function PlacesBackground_dispatch(aEvent, aFlags)
  {
    this._thread.dispatch(aEvent, aFlags);
  },

  isOnCurrentThread: function PlacesBackground_isOnCurrentThread()
  {
    return this._thread.isOnCurrentThread();
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIObserver

  observe: function PlacesBackground_observe(aSubject, aTopic, aData)
  {
    if (aTopic == kQuitApplication) {
      // Notify consumers that we are shutting down.
      let os = Cc["@mozilla.org/observer-service;1"].
               getService(Ci.nsIObserverService);
      os.notifyObservers(null, kPlacesBackgroundShutdown, null);

      // Now shut the thread down.
      this._thread.shutdown();
      this._thread = null;
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIEventTarget,
    Ci.nsIObserver,
  ])
};


__defineGetter__("PlacesBackground", function() {
  delete this.PlacesBackground;
  return this.PlacesBackground = new nsPlacesBackground;
});
