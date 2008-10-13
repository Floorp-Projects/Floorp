/*
 *  getNotificationBox
 *
 * Fetches the notification box for the specified window.
 */
function getNotificationBox(aWindow) {
    var chromeWin = aWindow
                        .QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebNavigation)
                        .QueryInterface(Ci.nsIDocShellTreeItem)
                        .rootTreeItem
                        .QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDOMWindow)
                        .QueryInterface(Ci.nsIDOMChromeWindow);

    // Don't need .wrappedJSObject here, unlike when chrome does this.
    var notifyBox = chromeWin.getNotificationBox(aWindow);
    return notifyBox;
}


/*
 * getNotificationBar
 *
 */
function getNotificationBar(aBox, aKind) {
    ok(true, "Looking for " + aKind + " notification bar");
    // Sometimes callers wants a bar, sometimes not. Allow 0 or 1, but not 2+.
    ok(aBox.allNotifications.length <= 1, "Checking for multiple notifications");
    return aBox.getNotificationWithValue(aKind);
}


/*
 * clickNotificationButton
 *
 * Clicks the specified notification button.
 */
function clickNotificationButton(aBar, aButtonName) {
    // This is a bit of a hack. The notification doesn't have an API to
    // trigger buttons, so we dive down into the implementation and twiddle
    // the buttons directly.
    var buttons = aBar.getElementsByTagName("button");
    var clicked = false;
    for (var i = 0; i < buttons.length; i++) {
        if (buttons[i].label == aButtonName) {
            buttons[i].click();
            clicked = true;
            break;
        }
    }

    ok(clicked, "Clicked \"" + aButtonName + "\" button"); 
}
