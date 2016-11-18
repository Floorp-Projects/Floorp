/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported startup, shutdown */

var hunspell, dir;

function startup(data) {
  hunspell = Components.classes["@mozilla.org/spellchecker/engine;1"]
                       .getService(Components.interfaces.mozISpellCheckingEngine);
  dir = data.installPath.clone();
  dir.append("dictionaries");
  hunspell.addDirectory(dir);
}

function shutdown() {
  hunspell.removeDirectory(dir);
}
