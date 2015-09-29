// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");

function onLoadViewPartialSource() {
  // check the view_source.wrap_long_lines pref
  // and set the menuitem's checked attribute accordingly
  let wrapLongLines = Services.prefs.getBoolPref("view_source.wrap_long_lines");
  document.getElementById("menu_wrapLongLines")
          .setAttribute("checked", wrapLongLines);
  document.getElementById("menu_highlightSyntax")
          .setAttribute("checked",
                        Services.prefs.getBoolPref("view_source.syntax_highlight"));

  let args = window.arguments[0];
  viewSourceChrome.loadViewSourceFromSelection(args.URI, args.drawSelection, args.baseURI);
  window.content.focus();
}
