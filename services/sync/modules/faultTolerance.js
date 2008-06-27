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

const Cu = Components.utils;
Cu.import("resource://weave/log4moz.js");

const EXPORTED_SYMBOLS = ["FaultTolerance"];

FaultTolerance = {
  get Service() {
    if (!this._Service)
      this._Service = new FaultToleranceService();
    return this._Service;
  }
};

function FaultToleranceService() {
}

FaultToleranceService.prototype = {
  init: function FTS_init() {
    var appender = new Appender();

    Log4Moz.Service.rootLogger.addAppender(appender);
  }
};

function Formatter() {
}
Formatter.prototype = {
  format: function FTF_format(message) {
    return message;
  }
};
Formatter.prototype.__proto__ = new Log4Moz.Formatter();

function Appender() {
  this._name = "FaultToleranceAppender";
  this._formatter = new Formatter();
}
Appender.prototype = {
  doAppend: function FTA_append(message) {
    // TODO: Implement this.
  }
};
Appender.prototype.__proto__ = new Log4Moz.Appender();
