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

        appendMessage({ message:
                        "Unable to display errors - " +
                        "couldn't get Console Service component. " +
                        "(Missing mozilla.consoleservice.1)" });
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

    return true;
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

    return true;
}

/*
 * Given a message, write it to the page.
 *
 * TODO: (When interesting subclasses of message objects become available) -
 * reflect structure of messages (e.g. lineno, etc.) as CSS-decorable structure.
 */
function appendMessage(messageObject)
{
    var c = document.getElementById("consoleTreeChildren");
    var item = document.createElement("treeitem");
    var row = document.createElement("treerow");
    var cell = document.createElement("treecell");
    cell.setAttribute("class", "treecell-error");
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
         
        cell.setAttribute("type", warning ? "warning" : "error");
        // XXX localize
        cell.setAttribute("typetext", warning ? "Warning: " : "Error: ");
        cell.setAttribute("url", scriptError.sourceName);
        cell.setAttribute("line", scriptError.lineNumber);
        cell.setAttribute("col", scriptError.columnNumber);
        cell.setAttribute("msg", scriptError.message);
        cell.setAttribute("error", scriptError.sourceLine);
    } catch (exn) {
        dump(exn + '\n');
        // QI failed, just try to treat it as an nsIConsoleMessage
        text = messageObject.message;
        cell.setAttribute("value", text);
    }
    row.appendChild(cell);
    item.appendChild(row);
    c.appendChild(item);
}

function changeMode (aMode, aElement)
{
  var broadcaster = document.getElementById(aElement.getAttribute("observes"));
  var bcset = document.getElementById("broadcasterset");
  for (var i = 0; i < bcset.childNodes.length; i++) {
    bcset.childNodes[i].removeAttribute("toggled");
    bcset.childNodes[i].removeAttribute("checked");
  }
  broadcaster.setAttribute("toggled", "true");
  broadcaster.setAttribute("checked", "true");
  var tree = document.getElementById("console");
  switch (aMode) {
    case "errors":
      tree.setAttribute("mode", "errors");
      break;
    case "warnings":
      tree.setAttribute("mode", "warnings");
      break;
    default:
    case "all":
      tree.removeAttribute("mode");
      break;
  }
}

// XXX q: if window is open, does it grow forever?  Is that OK?
// or should it do its own retiring?
