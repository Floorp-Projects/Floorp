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
 *  Dan Mills <thunder@mozilla.com>
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
Cu.import("resource://weave/util.js");

const EXPORTED_SYMBOLS = ["FaultTolerance"];

FaultTolerance = {
  get Service() {
    if (!this._Service)
      this._Service = new FTService();
    return this._Service;
  }
};

function FTService() {
  this._log = Log4Moz.repository.getLogger("FaultTolerance");
  this._appender = new FTAppender(this);
  Log4Moz.repository.rootLogger.addAppender(this._appender);
}
FTService.prototype = {
  get lastException() this._lastException,
  onMessage: function FTS_onMessage(message) {
    // FIXME: we get all log messages here, and could use them to keep track of
    // our current state
  },
  onException: function FTS_onException(exception) {
    this._lastException = exception;
    this._log.debug("\n" + Utils.stackTrace(exception));
    return true; // continue sync if thrown by a sync engine
  }
};

function FTFormatter() {}
FTFormatter.prototype = {
  __proto__: new Log4Moz.Formatter(),
  format: function FTF_format(message) message
};

function FTAppender(ftService) {
  this._ftService = ftService;
  this._name = "FTAppender";
  this._formatter = new FTFormatter();
}
FTAppender.prototype = {
  __proto__: new Log4Moz.Appender(),
  doAppend: function FTA_append(message) {
    this._ftService.onMessage(message);
  }
};
