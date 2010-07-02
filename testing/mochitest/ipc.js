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
 * The Original Code is Mozilla's layout acceptance tests.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joel Maher <joel.maher@gmail.com>, Mozilla Corporation (original author)
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

if (Cc === undefined) {
  var Cc = Components.classes;
  var Ci = Components.interfaces;
}

function ipcEvent(e) {
    var sync = e.getData("sync");
    var type = e.getData("type");
    var data = JSON.parse(e.getData("data"));

    switch(type) {
    case 'LoggerInit':
      MozillaFileLogger.init(data.filename);
      break;
    case 'Logger':
      var logger = MozillaFileLogger.getLogCallback();
      logger({"num":data.num, "level":data.level, "info": Array(data.info)});
      break;
    case 'LoggerClose':
      MozillaFileLogger.close();
      break;
    case 'waitForFocus':
      if (content)
        var wrapper = content.wrappedJSObject.frames[0].SimpleTest;
      else
        var wrapper = SimpleTest;
      ipctest.waitForFocus(wrapper[data.callback], data.targetWindow, data.expectBlankPage);
      break;
    default:
      if (type == 'QuitApplication') {
        removeEventListener("contentEvent", function (e) { ipcEvent(e); }, false, true);
      }

      if (sync == 1) {
        return sendSyncMessage("chromeEvent", {"type":type, "data":data});
      } else {
        sendAsyncMessage("chromeEvent", {"type":type, "data":data});
      }
    }
};

var xulRuntime = Components.classes["@mozilla.org/xre/app-info;1"]
                 .getService(Components.interfaces.nsIXULRuntime);

//check if we are in content process
if (xulRuntime.processType == 2) {
  addEventListener("contentEvent", function (e) { ipcEvent(e); }, false, true);
}

var ipctest = {};

ipctest.waitForFocus_started = false;
ipctest.waitForFocus_loaded = false;
ipctest.waitForFocus_focused = false;

ipctest.waitForFocus = function (callback, targetWindow, expectBlankPage) {

    if (targetWindow && targetWindow != undefined) {
      var tempID = targetWindow;
      targetWindow = null;
      var wm = Cc["@mozilla.org/appshell/window-mediator;1"]
                         .getService(Ci.nsIWindowMediator);
      var wm_enum = wm.getXULWindowEnumerator(null);

      while(wm_enum.hasMoreElements()) {
        var win = wm_enum.getNext().QueryInterface(Ci.nsIXULWindow)
                  .docShell.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow)
                  .content.wrappedJSObject;

        var domutils = win.QueryInterface(Ci.nsIInterfaceRequestor).
                           getInterface(Ci.nsIDOMWindowUtils);

        if (domutils.outerWindowID == tempID) {
          targetWindow = win;
        }
      }
    }

    if (targetWindow == null || targetWindow == undefined)
      if (content)
        targetWindow = content.document.defaultView;
      else
        targetWindow = window;

    ipctest.waitForFocus_started = false;
    expectBlankPage = !!expectBlankPage;

    var fm = Cc["@mozilla.org/focus-manager;1"].
                        getService(Ci.nsIFocusManager);

    var childTargetWindow = { };
    fm.getFocusedElementForWindow(targetWindow, true, childTargetWindow);
    childTargetWindow = childTargetWindow.value;

    function debugFocusLog(prefix) {
        if (content)
          var wrapper = content.wrappedJSObject.frames[0].SimpleTest;
        else
          var wrapper = SimpleTest;
        wrapper.ok(true, prefix + " -- loaded: " + targetWindow.document.readyState +
           " active window: " +
               (fm.activeWindow ? "(" + fm.activeWindow + ") " + fm.activeWindow.location : "<no window active>") +
           " focused window: " +
               (fm.focusedWindow ? "(" + fm.focusedWindow + ") " + fm.focusedWindow.location : "<no window focused>") +
           " desired window: (" + targetWindow + ") " + targetWindow.location +
           " child window: (" + childTargetWindow + ") " + childTargetWindow.location);
    }

    debugFocusLog("before wait for focus");

    function maybeRunTests() {
        debugFocusLog("maybe run tests <load:" +
                      ipctest.waitForFocus_loaded + ", focus:" + ipctest.waitForFocus_focused + ">");
        if (ipctest.waitForFocus_loaded &&
            ipctest.waitForFocus_focused &&
            !ipctest.waitForFocus_started) {
            ipctest.waitForFocus_started = true;
            if (content)
              content.setTimeout(function() { callback(); }, 0, targetWindow);
            else
              setTimeout(function() { callback(); }, 0, targetWindow);
        }
    }

    function waitForEvent(event) {
        try {
            debugFocusLog("waitForEvent called <type:" + event.type + ", target" + event.target + ">");

            // Check to make sure that this isn't a load event for a blank or
            // non-blank page that wasn't desired.
            if (event.type == "load" && (expectBlankPage != (event.target.location == "about:blank")))
                return;

            ipctest["waitForFocus_" + event.type + "ed"] = true;
            var win = (event.type == "load") ? targetWindow : childTargetWindow;
            win.removeEventListener(event.type, waitForEvent, true);
            maybeRunTests();
        } catch (e) {
        }
    }

    // If the current document is about:blank and we are not expecting a blank
    // page (or vice versa), and the document has not yet loaded, wait for the
    // page to load. A common situation is to wait for a newly opened window
    // to load its content, and we want to skip over any intermediate blank
    // pages that load. This issue is described in bug 554873.
    ipctest.waitForFocus_loaded =
        (expectBlankPage == (targetWindow.location == "about:blank")) &&
        targetWindow.document.readyState == "complete";
    if (!ipctest.waitForFocus_loaded) {
        targetWindow.addEventListener("load", waitForEvent, true);
    }

    // Check if the desired window is already focused.
    var focusedChildWindow = { };
    if (fm.activeWindow) {
        fm.getFocusedElementForWindow(fm.activeWindow, true, focusedChildWindow);
        focusedChildWindow = focusedChildWindow.value;
    }

    // If this is a child frame, ensure that the frame is focused.
    ipctest.waitForFocus_focused = (focusedChildWindow == childTargetWindow);
    if (ipctest.waitForFocus_focused) {
        maybeRunTests();
    }
    else {
        //TODO: is this really the childTargetWindow 
        //      and are we really doing something with it here?
        if (content) {
          var wr = childTargetWindow.wrappedJSObject;
          wr.addEventListener("focus", waitForEvent, true);
          sendAsyncMessage("chromeEvent", {"type":"Focus", "data":{}});
          wr.focus();
        } else {
          childTargetWindow.addEventListener("focus", waitForEvent, true);
          childTargetWindow.focus();
        }
    }
};
