/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*
 * Functions for initializing and updating the JavaScript Console.
 */

/*
 * Listener to be registered with the console service, called on new errors.
 */
var consoleListener = {
    observe : function(messageObject) {
        appendMessage(messageObject);
    }
}

/*
 * Gets the current context of the console service and dumps it to the window,
 * and registers a listener with the service to be called on additional errors.
 */
function onLoadJSConsole() 
{
    try {
        var cs_class = Components.classes['consoleservice'];
        var cs_iface = Components.interfaces.nsIConsoleService;
        var cs_isupports = cs_class.getService();
        var cs = cs_isupports.QueryInterface(cs_iface);
    } catch(exn) {
        // Couldn't get the console service for some reason...
        // pretend it never happened.

        // XXX instead should write some message to the console re:
        // unable to get console service.

        return;
    }

    var o1 = {}; // Throwaway references to support 'out' parameters.
    var o2 = {};

    var messages;
    cs.getMessageArray(o1, o2);
    messages = o1.value;

    // In case getMessageArray returns 0-length array as null.
    if (messages == null)
        messages = [];
    
    for (i = 0; i < messages.length; i++) {
        appendMessage(messages[i]);
    }

    cs.registerListener(consoleListener);
}

function onUnloadJSConsole()
{
    // keep a global ref. to it instead???

    try {
        var cs_class = Components.classes['consoleservice'];
        var cs_iface = Components.interfaces.nsIConsoleService;
        var cs_isupports = cs_class.getService();
        var cs = cs_isupports.QueryInterface(cs_iface);
    } catch(exn) {
        // Couldn't get the console service for some reason...
        // pretend it never happened.
        return;
    }

    cs.unregisterListener(consoleListener);
}

/*
 * Given a message, write it to the page.
 *
 * TODO: (When interesting subclasses of message objects become available) -
 * reflect structure of messages (e.g. lineno, etc.) as CSS-decorable structure.
 */
function appendMessage(messageObject)
{
    var c = document.getElementById("console");
    var e = document.createElement("message");
    var text;
    try {
        // Try to QI it to a script error to get more info.
        var scripterror =
            messageObject.QueryInterface(Components.interfaces.nsIScriptError);

        text = scripterror.sourceName + " line " + scripterror.lineNumber +
            ": " + scripterror.message;
    } catch (exn) {
        // QI failed, just try to treat it as an nsIConsoleMessage
        text = messageObject.message;
    }

    var t = document.createTextNode(text);
    e.appendChild(t);
    c.appendChild(e);
}

// XXX q: if window is open, does it grow forever?  Is that OK?
// or should it do its own retiring?
