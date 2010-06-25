/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License
 * at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and
 * limitations under the License.
 *
 * The Original Code is Places Dynamic Containers unit test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Asaf Romano <mano@mozilla.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the
 * terms of either the GNU General Public License Version 2 or later
 * (the "GPL"), or the GNU Lesser General Public License Version 2.1
 * or later (the "LGPL"), in which case the provisions of the GPL or
 * the LGPL are applicable instead of those above. If you wish to
 * allow use of your version of this file only under the terms of
 * either the GPL or the LGPL, and not to allow others to use your
 * version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the
 * notice and other provisions required by the GPL or the LGPL. If you
 * do not delete the provisions above, a recipient may use your
 * version of this file under the terms of any one of the MPL, the GPL
 * or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function RemoteContainerSampleService() {
}

RemoteContainerSampleService.prototype = {

  get bms() {
    if (!this._bms)
      this._bms = Cc[BMS_CONTRACTID].getService(Ci.nsINavBookmarksService);
    return this._bms;
  },

  get history() {
    if (!this._history)
      this._history = Cc[NH_CONTRACTID].getService(Ci.nsINavHistoryService);
    return this._history;
  },

  // IO Service
  get ios() {
    if (!this._ios)
      this._ios = Cc["@mozilla.org/network/io-service;1"].
                   getService(Ci.nsIIOService);
    return this._ios;
  },

  // nsIDynamicContainer
  onContainerRemoving: function(container) { },

  onContainerMoved: function() { },

  onContainerNodeOpening: function(container, options) {
    container.appendURINode("http://foo.tld/", "uri 1", 0, 0, null);

    var folder = Cc["@mozilla.org/browser/annotation-service;1"].
                 getService(Ci.nsIAnnotationService).
                 getItemAnnotation(container.itemId, "exposedFolder");

    container.appendFolderNode(folder);
  },

  onContainerNodeClosed: function() { },

  createInstance: function LS_createInstance(outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return this.QueryInterface(iid);
  },

  classID: Components.ID("{0d42adc5-f07a-4da2-b8da-3e2ef114cb67}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDynamicContainer]),
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([RemoteContainerSampleService]);
