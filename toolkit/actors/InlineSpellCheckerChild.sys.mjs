/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  InlineSpellCheckerContent:
    "resource://gre/modules/InlineSpellCheckerContent.sys.mjs",
});

export class InlineSpellCheckerChild extends JSWindowActorChild {
  receiveMessage(msg) {
    switch (msg.name) {
      case "InlineSpellChecker:selectDictionaries":
        lazy.InlineSpellCheckerContent.selectDictionaries(msg.data.localeCodes);
        break;

      case "InlineSpellChecker:replaceMisspelling":
        lazy.InlineSpellCheckerContent.replaceMisspelling(msg.data.suggestion);
        break;

      case "InlineSpellChecker:toggleEnabled":
        lazy.InlineSpellCheckerContent.toggleEnabled();
        break;

      case "InlineSpellChecker:recheck":
        lazy.InlineSpellCheckerContent.recheck();
        break;

      case "InlineSpellChecker:uninit":
        lazy.InlineSpellCheckerContent.uninitContextMenu();
        break;
    }
  }
}
