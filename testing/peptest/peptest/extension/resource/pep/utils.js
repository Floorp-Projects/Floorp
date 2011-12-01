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
* The Original Code is peptest.
*
* The Initial Developer of the Original Code is
*   The Mozilla Foundation.
* Portions created by the Initial Developer are Copyright (C) 2011.
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
*   Andrew Halberstadt <halbersa@gmail.com>
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
***** END LICENSE BLOCK ***** */

var EXPORTED_SYMBOLS = ['readFile', 'sleep'];

const consoleService = Components.classes["@mozilla.org/consoleservice;1"]
                                 .getService(Components.interfaces.nsIConsoleService);
const hwindow = Components.classes["@mozilla.org/appshell/appShellService;1"]
                          .getService(Components.interfaces.nsIAppShellService)
                          .hiddenDOMWindow;

/**
 * Reads in the local file at filepath and returns a list
 * of all lines that were read in
 */
function readFile(filePath) {
  let file = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);
  file.initWithPath(filePath);

  const fstream = Components.classes["@mozilla.org/network/file-input-stream;1"]
                            .createInstance(Components.interfaces.nsIFileInputStream);
  const cstream = Components.classes["@mozilla.org/intl/converter-input-stream;1"]
                            .createInstance(Components.interfaces.nsIConverterInputStream);
  fstream.init(file, -1, 0, 0);
  cstream.init(fstream, "UTF-8", 0, 0);

  let data = [];
  let (str = {}) {
    let read = 0;
    do {
      read = cstream.readString(0xffffffff, str);
      data.push(str.value);
    } while (read != 0);
  }
  cstream.close();  // Also closes fstream
  return data;
};

/**
 * Sleep for the given amount of milliseconds
 *
 * @param {number} milliseconds
 * Sleeps the given number of milliseconds
 */
function sleep(milliseconds) {
  // We basically just call this once after the specified number of milliseconds
  var timeup = false;
  function wait() { timeup = true; }
  hwindow.setTimeout(wait, milliseconds);

  var thread = Components.classes["@mozilla.org/thread-manager;1"].
               getService().currentThread;
  while(!timeup) {
    thread.processNextEvent(true);
  }
};
