/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {CommonDialog} = ChromeUtils.import("resource://gre/modules/CommonDialog.jsm");

var propBag, args, Dialog;

function commonDialogOnLoad() {
    propBag = window.arguments[0].QueryInterface(Ci.nsIWritablePropertyBag2)
                                 .QueryInterface(Ci.nsIWritablePropertyBag);
    // Convert to a JS object
    args = {};
    for (let prop of propBag.enumerator) {
        args[prop.name] = prop.value;
    }

    let dialog = document.documentElement;

    let ui = {
        prompt: window,
        loginContainer: document.getElementById("loginContainer"),
        loginTextbox: document.getElementById("loginTextbox"),
        loginLabel: document.getElementById("loginLabel"),
        password1Container: document.getElementById("password1Container"),
        password1Textbox: document.getElementById("password1Textbox"),
        password1Label: document.getElementById("password1Label"),
        infoBody: document.getElementById("infoBody"),
        infoTitle: document.getElementById("infoTitle"),
        infoIcon: document.getElementById("infoIcon"),
        checkbox: document.getElementById("checkbox"),
        checkboxContainer: document.getElementById("checkboxContainer"),
        button3: dialog.getButton("extra2"),
        button2: dialog.getButton("extra1"),
        button1: dialog.getButton("cancel"),
        button0: dialog.getButton("accept"),
        focusTarget: window,
    };

    // limit the dialog to the screen width
    document.getElementById("filler").maxWidth = screen.availWidth;

    Dialog = new CommonDialog(args, ui);
    document.addEventListener("dialogaccept", function() { Dialog.onButton0(); });
    document.addEventListener("dialogcancel", function() { Dialog.onButton1(); });
    document.addEventListener("dialogextra1", function() { Dialog.onButton2(); window.close(); });
    document.addEventListener("dialogextra2", function() { Dialog.onButton3(); window.close(); });
    Dialog.onLoad(dialog);

    // resize the window to the content
    window.sizeToContent();
    window.getAttention();
}

function commonDialogOnUnload() {
    // Convert args back into property bag
    for (let propName in args)
        propBag.setProperty(propName, args[propName]);
}
