/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const flavor  = __webDriverArguments[0]
const url = __webDriverArguments[1]

let wm = Cc["@mozilla.org/appshell/window-mediator;1"]
          .getService(Ci.nsIWindowMediator);
let win = wm.getMostRecentWindow("navigator:browser");

// mochikit's bootstrap.js has set up a listener for this event. It's
// used so bootstrap.js knows which flavor and url to load.
let ev = new CustomEvent('mochitest-load', {'detail': [flavor, url]});
win.dispatchEvent(ev);
