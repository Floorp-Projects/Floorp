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
 * The Original Code is Bookmarks Sync.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Atul Varma <varmaa@toolness.com>
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

EXPORTED_SYMBOLS = ["Sharing"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://weave/async.js");
Cu.import("resource://weave/identity.js");

Function.prototype.async = Async.sugar;

function Api(dav) {
  this._dav = dav;
}

Api.prototype = {
  shareWithUsers: function Api_shareWithUsers(path, users, onComplete) {
    this._shareGenerator.async(this,
                               onComplete,
                               path,
                               users);
  },

  _shareGenerator: function Api__shareGenerator(path, users) {
    let self = yield;
    let id = ID.get(this._dav.identity);

    this._dav.defaultPrefix = "";

    let cmd = {"version" : 1,
               "directory" : path,
               "share_to_users" : users};
    let jsonSvc = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
    let json = jsonSvc.encode(cmd);

    this._dav.POST("share/",
                   ("cmd=" + escape(json) +
                    "&uid=" + escape(id.username) +
                    "&password=" + escape(id.password)),
                   self.cb);
    let xhr = yield;

    let retval;

    if (xhr.status == 200) {
      if (xhr.responseText == "OK") {
        retval = {wasSuccessful: true};
      } else {
        retval = {wasSuccessful: false,
                  errorText: xhr.responseText};
      }
    } else {
      retval = {wasSuccessful: false,
                errorText: "Server returned status " + xhr.status};
    }

    self.done(retval);
  }
};

Sharing = {
  Api: Api
};
