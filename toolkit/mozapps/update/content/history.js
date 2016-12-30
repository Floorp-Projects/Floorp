/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const NS_XUL  = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

var gUpdateHistory = {
  _view: null,

  /**
   * Initialize the User Interface
   */
  onLoad() {
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
  _formatDate(seconds) {
    var date = new Date(seconds);
    const locale = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
                   .getService(Components.interfaces.nsIXULChromeRegistry)
                   .getSelectedLocale("global", true);
    const dtOptions = { year: 'numeric', month: 'long', day: 'numeric',
                        hour: 'numeric', minute: 'numeric', second: 'numeric' };
    return date.toLocaleString(locale, dtOptions);
  }
};

