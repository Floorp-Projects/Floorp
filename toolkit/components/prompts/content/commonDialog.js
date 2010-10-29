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
 *   Alec Flett  <alecf@netscape.com>
 *   Ben Goodger <ben@netscape.com>
 *   Blake Ross  <blakeross@telocity.com>
 *   Joe Hewitt <hewitt@netscape.com>
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

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/CommonDialog.jsm");

let propBag, args, Dialog;

function commonDialogOnLoad() {
    propBag = window.arguments[0].QueryInterface(Ci.nsIWritablePropertyBag2)
                                 .QueryInterface(Ci.nsIWritablePropertyBag);
    // Convert to a JS object
    args = {};
    let propEnum = propBag.enumerator;
    while (propEnum.hasMoreElements()) {
        let prop = propEnum.getNext().QueryInterface(Ci.nsIProperty);
        args[prop.name] = prop.value;
    }

    let dialog = document.documentElement;

    let ui = {
        loginContainer     : document.getElementById("loginContainer"),
        loginTextbox       : document.getElementById("loginTextbox"),
        loginLabel         : document.getElementById("loginLabel"),
        password1Container : document.getElementById("password1Container"),
        password1Textbox   : document.getElementById("password1Textbox"),
        password1Label     : document.getElementById("password1Label"),
        infoBody           : document.getElementById("info.body"),
        infoTitle          : document.getElementById("info.title"),
        infoIcon           : document.getElementById("info.icon"),
        checkbox           : document.getElementById("checkbox"),
        checkboxContainer  : document.getElementById("checkboxContainer"),
        button3            : dialog.getButton("extra2"),
        button2            : dialog.getButton("extra1"),
        button1            : dialog.getButton("cancel"),
        button0            : dialog.getButton("accept"),
        focusTarget        : window,
    };

    // limit the dialog to the screen width
    document.getElementById("filler").maxWidth = screen.availWidth;
    Services.obs.addObserver(softkbObserver, "softkb-change", false);

    Dialog = new CommonDialog(args, ui);
    Dialog.onLoad(dialog);
    window.getAttention();
}

function commonDialogOnUnload() {
    Services.obs.removeObserver(softkbObserver, "softkb-change");
    // Convert args back into property bag
    for (let propName in args)
        propBag.setProperty(propName, args[propName]);
}

function softkbObserver(subject, topic, data) {
    let rect = JSON.parse(data);
    if (rect) {
        let height = rect.bottom - rect.top;
        let width  = rect.right - rect.left;
        let top    = (rect.top + (height - window.innerHeight) / 2);
        let left   = (rect.left + (width - window.innerWidth) / 2);
        window.moveTo(left, top);
    }
}
