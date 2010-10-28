/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
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
 * The Original Code is Places code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (Original Author)
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

////////////////////////////////////////////////////////////////////////////////
//// Constants

const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

/**
 * This component can be used as a starter for modules that have to run when
 * certain categories are invoked.
 */
function PlacesCategoriesStarter()
{
}

PlacesCategoriesStarter.prototype = {
  //////////////////////////////////////////////////////////////////////////////
  //// nsIObserver

  observe: function PCS_observe(aSubject, aTopic, aData)
  {
    switch (aTopic) {
      case "idle-daily":
        // Run places.sqlite maintenance tasks.
        Components.utils.import("resource://gre/modules/PlacesDBUtils.jsm");
        PlacesDBUtils.maintenanceOnIdle();
        break;
      default:
        throw new Error("Trying to handle an unknown category.");
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  classID: Components.ID("803938d5-e26d-4453-bf46-ad4b26e41114"),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIObserver
  ])
};

////////////////////////////////////////////////////////////////////////////////
//// Module Registration

let components = [PlacesCategoriesStarter];
var NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
