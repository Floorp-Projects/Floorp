/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["InlineSpellCheckerChild"];

ChromeUtils.defineModuleGetter(
  this,
  "InlineSpellCheckerContent",
  "resource://gre/modules/InlineSpellCheckerContent.jsm"
);

class InlineSpellCheckerChild extends JSWindowActorChild {
  receiveMessage(msg) {
    switch (msg.name) {
      case "InlineSpellChecker:selectDictionary":
        InlineSpellCheckerContent.selectDictionary(msg.data.localeCode);
        break;

      case "InlineSpellChecker:replaceMisspelling":
        InlineSpellCheckerContent.replaceMisspelling(msg.data.suggestion);
        break;

      case "InlineSpellChecker:toggleEnabled":
        InlineSpellCheckerContent.toggleEnabled();
        break;

      case "InlineSpellChecker:recheck":
        InlineSpellCheckerContent.recheck();
        break;

      case "InlineSpellChecker:uninit":
        InlineSpellCheckerContent.uninitContextMenu();
        break;
    }
  }
}
