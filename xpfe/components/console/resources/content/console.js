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
        var cs_class = Components.classes['mozilla.consoleservice.1'];
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
        var cs_class = Components.classes['mozilla.consoleservice.1'];
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
    var el = document.createElement("message");
    var msgContent;
    var text;
    try {
        // Try to QI it to a script error to get more info.
        var nsIScriptError = Components.interfaces.nsIScriptError;
        var scriptError =
            messageObject.QueryInterface(nsIScriptError);

        // Is this error actually just a non-fatal warning?
        var warning = scriptError.flags & nsIScriptError.warningFlag != 0;

        // Is this error or warning a result of JavaScript's strict mode,
        // and therefore something we might decide to be lenient about?
        var strict = scriptError.flags & nsIScriptError.strictFlag != 0;

        // If true, this error led to the creation of a (catchable!) javascript
        // exception, and should probably be ignored.
        // This is worth checking, as I'm not sure how the behavior falls
        // out.  Does an uncaught exception result in this flag being raised?
        var isexn = scriptError.flags & nsIScriptError.exceptionFlag != 0;

        /*
         * It'd be nice to do something graphical with our column information,
         * like what the old js console did, and the js shell does:
         * js> var scooby = "I am an unterminated string!
         * 1: unterminated string literal:
         * 1: var scooby = "I am an unterminated string!
         * 1: .............^
         *
         * But for now, I'm just pushing it out the door.
         */
        text = scriptError.category + ": ";
        text += "JavaScript " + (warning ? "Warning: " : "Error: ");
        text += scriptError.sourceName;
        text += " line " + scriptError.lineNumber +
            ", column " + scriptError.columnNumber + ": " + scriptError.message;
        text += " Source line: " + scriptError.sourceLine;

        msgContent = document.createTextNode(text);
    } catch (exn) {
        dump(exn + '\n');
        // QI failed, just try to treat it as an nsIConsoleMessage
        text = messageObject.message;
        msgContent = document.createTextNode(text);
    }

    el.appendChild(msgContent);
    c.appendChild(el);
}

// XXX q: if window is open, does it grow forever?  Is that OK?
// or should it do its own retiring?
