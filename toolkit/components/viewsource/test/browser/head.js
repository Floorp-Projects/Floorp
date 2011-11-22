/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function openViewSourceWindow(aURI, aCallback) {
  let viewSourceWindow = openDialog("chrome://global/content/viewSource.xul", null, null, aURI);
  viewSourceWindow.addEventListener("pageshow", function pageShowHandler(event) {
    if (event.target.location == "view-source:" + aURI) {
      info("View source window opened: " + event.target.location);
      viewSourceWindow.removeEventListener("pageshow", pageShowHandler, false);
      aCallback(viewSourceWindow);
    }
  }, false);
}

function closeViewSourceWindow(aWindow, aCallback) {
  Services.wm.addListener({
    onCloseWindow: function() {
      Services.wm.removeListener(this);
      aCallback();
    }
  });
  aWindow.close();
}

registerCleanupFunction(function() {
  var windows = Services.wm.getEnumerator("navigator:view-source");
  if (windows.hasMoreElements())
    ok(false, "Found unexpected view source window still open");
  while (windows.hasMoreElements())
    windows.getNext().QueryInterface(Components.interfaces.nsIDOMWindow).close();
});
