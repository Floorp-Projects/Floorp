/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
        prompt             : window,
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
