/**
 * Given some nsIDOMWindow for a window running in the parent
 * process, return the nsIWebBrowserChrome chrome flags for
 * the associated XUL window.
 *
 * @param win (nsIDOMWindow)
 *        Some window in the parent process.
 * @returns int
 */
function getParentChromeFlags(win) {
  return win.docShell.treeOwner
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIAppWindow).chromeFlags;
}
