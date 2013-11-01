/*
 * Initialization: for each test, remove any prior notifications.
 */
function cleanUpPopupNotifications() {
    var container = getPopupNotifications(window.top);
    var notes = container._currentNotifications;
    info(true, "Removing " + notes.length + " popup notifications.");
    for (var i = notes.length-1; i >= 0; i--) {
	notes[i].remove();
    }
}
cleanUpPopupNotifications();

/*
 * getPopupNotifications
 *
 * Fetches the popup notification for the specified window.
 */
function getPopupNotifications(aWindow) {
    var Ci = SpecialPowers.Ci;
    var Cc = SpecialPowers.Cc;
    ok(Ci != null, "Access Ci");
    ok(Cc != null, "Access Cc");

    var chromeWin = SpecialPowers.wrap(aWindow)
                                 .QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIWebNavigation)
                                 .QueryInterface(Ci.nsIDocShell)
                                 .chromeEventHandler.ownerDocument.defaultView;

    var popupNotifications = chromeWin.PopupNotifications;
    return popupNotifications;
}


/*
 * getPopup
 *
 */
function getPopup(aPopupNote, aKind) {
    ok(true, "Looking for " + aKind + " popup notification");
    return aPopupNote.getNotification(aKind);
}


/*
 * clickPopupButton
 *
 * Clicks the specified popup notification button.
 */
function clickPopupButton(aPopup, aButtonIndex) {
    ok(true, "Looking for action at index " + aButtonIndex);

    var notifications = SpecialPowers.wrap(aPopup.owner).panel.childNodes;
    ok(notifications.length > 0, "at least one notification displayed");
    ok(true, notifications.length + " notifications");
    var notification = notifications[0];

    if (aButtonIndex == 0) {
        ok(true, "Triggering main action");
        notification.button.doCommand();
    } else if (aButtonIndex <= aPopup.secondaryActions.length) {
        var index = aButtonIndex - 1;
        ok(true, "Triggering secondary action " + index);
        notification.childNodes[index].doCommand();
    }
}

const kRememberButton = 0;
const kNeverButton = 1;

const kChangeButton = 0;
const kDontChangeButton = 1;

function dumpNotifications() {
  try {
    // PopupNotifications
    var container = getPopupNotifications(window.top);
    ok(true, "is popup panel open? " + container.isPanelOpen);
    var notes = container._currentNotifications;
    ok(true, "Found " + notes.length + " popup notifications.");
    for (var i = 0; i < notes.length; i++) {
        ok(true, "#" + i + ": " + notes[i].id);
    }

    // Notification bars
    var chromeWin = SpecialPowers.wrap(window.top)
                           .QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIWebNavigation)
                           .QueryInterface(Ci.nsIDocShell)
                           .chromeEventHandler.ownerDocument.defaultView;
    var nb = chromeWin.getNotificationBox(window.top);
    var notes = nb.allNotifications;
    ok(true, "Found " + notes.length + " notification bars.");
    for (var i = 0; i < notes.length; i++) {
        ok(true, "#" + i + ": " + notes[i].getAttribute("value"));
    }
  } catch(e) { todo(false, "WOAH! " + e); }
}
