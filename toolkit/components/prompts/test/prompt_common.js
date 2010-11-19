netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

const Ci = Components.interfaces;
const Cc = Components.classes;
ok(Ci != null, "Access Ci");
ok(Cc != null, "Access Cc");

var didDialog;

var isSelectDialog = false;
var isTabModal = false;
var usePromptService = true;

var timer; // keep in outer scope so it's not GC'd before firing
function startCallbackTimer() {
    didDialog = false;

    // Delay before the callback twiddles the prompt.
    const dialogDelay = 10;

    // Use a timer to invoke a callback to twiddle the authentication dialog
    timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.init(observer, dialogDelay, Ci.nsITimer.TYPE_ONE_SHOT);
}


var observer = {
    QueryInterface : function (iid) {
        const interfaces = [Ci.nsIObserver,
                            Ci.nsISupports, Ci.nsISupportsWeakReference];

        if (!interfaces.some( function(v) { return iid.equals(v) } ))
            throw Components.results.NS_ERROR_NO_INTERFACE;
        return this;
    },

    observe : function (subject, topic, data) {
        netscape.security.PrivilegeManager
                         .enablePrivilege('UniversalXPConnect');

        if (isTabModal) {
          var promptBox = getTabModalPromptBox(window);
          ok(promptBox, "got tabmodal promptbox");
          var prompts = promptBox.listPrompts();
          if (prompts.length)
              handleDialog(prompts[0].Dialog.ui, testNum);
          else
              startCallbackTimer(); // try again in a bit
        } else {
          var doc = getDialogDoc();
          if (isSelectDialog && doc)
              handleDialog(doc, testNum);
          else if (doc)
              handleDialog(doc.defaultView.Dialog.ui, testNum);
          else
              startCallbackTimer(); // try again in a bit
        }
    }
};

function getTabModalPromptBox(domWin) {
    var promptBox = null;

    // Given a content DOM window, returns the chrome window it's in.
    function getChromeWindow(aWindow) {
        var chromeWin = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIWebNavigation)
                               .QueryInterface(Ci.nsIDocShell)
                               .chromeEventHandler.ownerDocument.defaultView;
        return chromeWin;
    }

    try {
        // Get topmost window, in case we're in a frame.
        var promptWin = domWin.top

        // Get the chrome window for the content window we're using.
        // .wrappedJSObject needed here -- see bug 422974 comment 5.
        var chromeWin = getChromeWindow(promptWin).wrappedJSObject;

        if (chromeWin.getTabModalPromptBox)
            promptBox = chromeWin.getTabModalPromptBox(promptWin);
    } catch (e) {
        // If any errors happen, just assume no tabmodal prompter.
    }

    return promptBox;
}

function getDialogDoc() {
  // Trudge through all the open windows, until we find the one
  // that has either commonDialog.xul or selectDialog.xul loaded.
  var wm = Cc["@mozilla.org/appshell/window-mediator;1"].
           getService(Ci.nsIWindowMediator);
  //var enumerator = wm.getEnumerator("navigator:browser");
  var enumerator = wm.getXULWindowEnumerator(null);

  while (enumerator.hasMoreElements()) {
    var win = enumerator.getNext();
    var windowDocShell = win.QueryInterface(Ci.nsIXULWindow).docShell;

    var containedDocShells = windowDocShell.getDocShellEnumerator(
                                      Ci.nsIDocShellTreeItem.typeChrome,
                                      Ci.nsIDocShell.ENUMERATE_FORWARDS);
    while (containedDocShells.hasMoreElements()) {
        // Get the corresponding document for this docshell
        var childDocShell = containedDocShells.getNext();
        // We don't want it if it's not done loading.
        if (childDocShell.busyFlags != Ci.nsIDocShell.BUSY_FLAGS_NONE)
          continue;
        var childDoc = childDocShell.QueryInterface(Ci.nsIDocShell)
                                    .contentViewer
                                    .DOMDocument;

        //ok(true, "Got window: " + childDoc.location.href);
        if (childDoc.location.href == "chrome://global/content/commonDialog.xul")
          return childDoc;
        if (childDoc.location.href == "chrome://global/content/selectDialog.xul")
          return childDoc;
    }
  }

  return null;
}
