/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Update Service.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Ben Goodger <ben@mozilla.org> (Original Author)
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

const NS_XUL  = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

var gUpdateHistory = {
  _view: null,
  
  /**
   * Initialize the User Interface
   */
  onLoad: function() {
    this._view = document.getElementById("historyItems");
    
    var um = 
        Components.classes["@mozilla.org/updates/update-manager;1"].
        getService(Components.interfaces.nsIUpdateManager);
    var uc = um.updateCount;
    if (uc) {
      while (this._view.hasChildNodes())
        this._view.removeChild(this._view.firstChild);
    
      var bundle = document.getElementById("updateBundle");
      
      for (var i = 0; i < uc; ++i) {
        var update = um.getUpdateAt(i);
        if (!update || !update.name)
          continue;

        // Don't display updates that are downloading since they don't have
        // valid statusText for the UI (bug 485493).
        if (!update.statusText)
          continue;

        var element = document.createElementNS(NS_XUL, "update");
        this._view.appendChild(element);
        element.name = bundle.getFormattedString("updateFullName", 
          [update.name, update.buildID]);
        element.type = bundle.getString("updateType_" + update.type);
        element.installDate = this._formatDate(update.installDate);
        if (update.detailsURL)
          element.detailsURL = update.detailsURL;
        else
          element.hideDetailsURL = true;
        element.status = update.statusText;
      }
    }
    var cancelbutton = document.documentElement.getButton("cancel");
    cancelbutton.focus();
  },
  
  /**
   * Formats a date into human readable form
   * @param   seconds
   *          A date in seconds since 1970 epoch
   * @returns A human readable date string
   */
  _formatDate: function(seconds) {
    var sdf = 
        Components.classes["@mozilla.org/intl/scriptabledateformat;1"].
        getService(Components.interfaces.nsIScriptableDateFormat);
    var date = new Date(seconds);
    return sdf.FormatDateTime("", sdf.dateFormatLong, 
                              sdf.timeFormatSeconds,
                              date.getFullYear(),
                              date.getMonth() + 1,
                              date.getDate(),
                              date.getHours(),
                              date.getMinutes(),
                              date.getSeconds());
  }
};

