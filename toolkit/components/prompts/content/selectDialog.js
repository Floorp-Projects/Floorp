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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
 *   Justin Dolske <dolske@mozilla.com>
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

const Ci = Components.interfaces;
const Cr = Components.results;
const Cc = Components.classes;
const Cu = Components.utils;

let gArgs, listBox;

function dialogOnLoad() {
    gArgs = window.arguments[0].QueryInterface(Ci.nsIWritablePropertyBag2)
                               .QueryInterface(Ci.nsIWritablePropertyBag);

    let promptType = gArgs.getProperty("promptType");
    if (promptType != "select") {
        Cu.reportError("selectDialog opened for unknown type: " + promptType);
        window.close();
    }

    // Default to canceled.
    gArgs.setProperty("ok", false);

    document.title = gArgs.getProperty("title");

    let text = gArgs.getProperty("text");
    document.getElementById("info.txt").setAttribute("value", text);

    let items = gArgs.getProperty("list");
    listBox = document.getElementById("list");

    for (let i = 0; i < items.length; i++) {
        let str = items[i];
        if (str == "")
            str = "<>";
        listBox.appendItem(str);
        listBox.getItemAtIndex(i).addEventListener("dblclick", dialogDoubleClick, false);
    }
    listBox.selectedIndex = 0;
    listBox.focus();

    // resize the window to the content
    window.sizeToContent();

    // Move to the right location
    moveToAlertPosition();
    centerWindowOnScreen();

    // play sound
    try {
        Cc["@mozilla.org/sound;1"].
        createInstance(Ci.nsISound).
        playEventSound(Ci.nsISound.EVENT_SELECT_DIALOG_OPEN);
    } catch (e) { }
}

function dialogOK() {
    let selected = listBox.selectedIndex;
    gArgs.setProperty("selected", listBox.selectedIndex);
    gArgs.setProperty("ok", true);
    return true;
}

function dialogDoubleClick() {
    dialogOK();
    window.close();
}
