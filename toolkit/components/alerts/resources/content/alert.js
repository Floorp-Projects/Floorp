/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// Copied from nsILookAndFeel.h, see comments on eMetric_AlertNotificationOrigin
const NS_ALERT_HORIZONTAL = 1;
const NS_ALERT_LEFT = 2;
const NS_ALERT_TOP = 4;

const WINDOW_MARGIN = AppConstants.platform == "win" ? 0 : 10;
const BODY_TEXT_LIMIT = 200;
const WINDOW_SHADOW_SPREAD = AppConstants.platform == "win" ? 10 : 0;


var gOrigin = 0; // Default value: alert from bottom right.
var gReplacedWindow = null;
var gAlertListener = null;
var gAlertTextClickable = false;
var gAlertCookie = "";
var gIsReplaced = false;
var gRequireInteraction = false;

function prefillAlertInfo() {
  // unwrap all the args....
  // arguments[0] --> the image src url
  // arguments[1] --> the alert title
  // arguments[2] --> the alert text
  // arguments[3] --> is the text clickable?
  // arguments[4] --> the alert cookie to be passed back to the listener
  // arguments[5] --> the alert origin reported by the look and feel
  // arguments[6] --> bidi
  // arguments[7] --> lang
  // arguments[8] --> requires interaction
  // arguments[9] --> replaced alert window (nsIDOMWindow)
  // arguments[10] --> an optional callback listener (nsIObserver)
  // arguments[11] -> the nsIURI.hostPort of the origin, optional
  // arguments[12] -> the alert icon URL, optional

  switch (window.arguments.length) {
    default:
    case 13: {
      if (window.arguments[12]) {
        let alertBox = document.getElementById("alertBox");
        alertBox.setAttribute("hasIcon", true);

        let icon = document.getElementById("alertIcon");
        icon.src = window.arguments[12];
      }
    }
    case 12: {
      if (window.arguments[11]) {
        let alertBox = document.getElementById("alertBox");
        alertBox.setAttribute("hasOrigin", true);

        let hostPort = window.arguments[11];
        const ALERT_BUNDLE = Services.strings.createBundle(
          "chrome://alerts/locale/alert.properties");
        const BRAND_BUNDLE = Services.strings.createBundle(
          "chrome://branding/locale/brand.properties");
        const BRAND_NAME = BRAND_BUNDLE.GetStringFromName("brandShortName");
        let label = document.getElementById("alertSourceLabel");
        label.setAttribute("value",
          ALERT_BUNDLE.formatStringFromName("source.label",
                                            [hostPort],
                                            1));
        let doNotDisturbMenuItem = document.getElementById("doNotDisturbMenuItem");
        doNotDisturbMenuItem.setAttribute("label",
          ALERT_BUNDLE.formatStringFromName("doNotDisturb.label",
                                            [BRAND_NAME],
                                            1));
        let disableForOrigin = document.getElementById("disableForOriginMenuItem");
        disableForOrigin.setAttribute("label",
          ALERT_BUNDLE.formatStringFromName("webActions.disableForOrigin.label",
                                            [hostPort],
                                            1));
        let openSettings = document.getElementById("openSettingsMenuItem");
        openSettings.setAttribute("label",
          ALERT_BUNDLE.GetStringFromName("webActions.settings.label"));
      }
    }
    case 11:
      gAlertListener = window.arguments[10];
    case 10:
      gReplacedWindow = window.arguments[9];
    case 9:
      gRequireInteraction = window.arguments[8];
    case 8:
      if (window.arguments[7]) {
        document.getElementById("alertTitleLabel").setAttribute("lang", window.arguments[7]);
        document.getElementById("alertTextLabel").setAttribute("lang", window.arguments[7]);
      }
    case 7:
      if (window.arguments[6]) {
        document.getElementById("alertNotification").style.direction = window.arguments[6];
      }
    case 6:
      gOrigin = window.arguments[5];
    case 5:
      gAlertCookie = window.arguments[4];
    case 4:
      gAlertTextClickable = window.arguments[3];
      if (gAlertTextClickable) {
        document.getElementById("alertNotification").setAttribute("clickable", true);
        document.getElementById("alertTextLabel").setAttribute("clickable", true);
      }
    case 3:
      if (window.arguments[2]) {
        document.getElementById("alertBox").setAttribute("hasBodyText", true);
        let bodyText = window.arguments[2];
        let bodyTextLabel = document.getElementById("alertTextLabel");

        if (bodyText.length > BODY_TEXT_LIMIT) {
          bodyTextLabel.setAttribute("tooltiptext", bodyText);

          let ellipsis = "\u2026";
          try {
            ellipsis = Services.prefs.getComplexValue("intl.ellipsis",
                                                      Ci.nsIPrefLocalizedString).data;
          } catch (e) { }

          // Copied from nsContextMenu.js' formatSearchContextItem().
          // If the JS character after our truncation point is a trail surrogate,
          // include it in the truncated string to avoid splitting a surrogate pair.
          let truncLength = BODY_TEXT_LIMIT;
          let truncChar = bodyText[BODY_TEXT_LIMIT].charCodeAt(0);
          if (truncChar >= 0xDC00 && truncChar <= 0xDFFF) {
            truncLength++;
          }

          bodyText = bodyText.substring(0, truncLength) +
                     ellipsis;
        }
        bodyTextLabel.textContent = bodyText;
      }
    case 2:
      document.getElementById("alertTitleLabel").setAttribute("value", window.arguments[1]);
    case 1:
      if (window.arguments[0]) {
        document.getElementById("alertBox").setAttribute("hasImage", true);
        document.getElementById("alertImage").setAttribute("src", window.arguments[0]);
      }
    case 0:
      break;
  }
}

function onAlertLoad() {
  const ALERT_DURATION_IMMEDIATE = 20000;
  let alertTextBox = document.getElementById("alertTextBox");
  let alertImageBox = document.getElementById("alertImageBox");
  alertImageBox.style.minHeight = alertTextBox.scrollHeight + "px";

  sizeToContent();

  if (gReplacedWindow && !gReplacedWindow.closed) {
    moveWindowToReplace(gReplacedWindow);
    gReplacedWindow.gIsReplaced = true;
    gReplacedWindow.close();
  } else {
    moveWindowToEnd();
  }

  window.addEventListener("XULAlertClose", function() { window.close(); });

  // If the require interaction flag is set, prevent auto-closing the notification.
  if (!gRequireInteraction) {
    if (!Services.prefs.getBoolPref("toolkit.cosmeticAnimations.enabled")) {
      setTimeout(function() { window.close(); }, ALERT_DURATION_IMMEDIATE);
    } else {
      let alertBox = document.getElementById("alertBox");
      alertBox.addEventListener("animationend", function hideAlert(event) {
        if (event.animationName == "alert-animation" ||
            event.animationName == "alert-clicked-animation" ||
            event.animationName == "alert-closing-animation") {
          alertBox.removeEventListener("animationend", hideAlert);
          window.close();
        }
      });
      alertBox.setAttribute("animate", true);
    }
  }

  let alertSettings = document.getElementById("alertSettings");
  alertSettings.addEventListener("focus", onAlertSettingsFocus);
  alertSettings.addEventListener("click", onAlertSettingsClick);

  let ev = new CustomEvent("AlertActive", {bubbles: true, cancelable: true});
  document.documentElement.dispatchEvent(ev);

  if (gAlertListener) {
    gAlertListener.observe(null, "alertshow", gAlertCookie);
  }
}

function moveWindowToReplace(aReplacedAlert) {
  let heightDelta = window.outerHeight - aReplacedAlert.outerHeight;

  // Move windows that come after the replaced alert if the height is different.
  if (heightDelta != 0) {
    let windows = Services.wm.getEnumerator("alert:alert");
    while (windows.hasMoreElements()) {
      let alertWindow = windows.getNext();
      // boolean to determine if the alert window is after the replaced alert.
      let alertIsAfter = gOrigin & NS_ALERT_TOP ?
                         alertWindow.screenY > aReplacedAlert.screenY :
                         aReplacedAlert.screenY > alertWindow.screenY;
      if (alertIsAfter) {
        // The new Y position of the window.
        let adjustedY = gOrigin & NS_ALERT_TOP ?
                        alertWindow.screenY + heightDelta :
                        alertWindow.screenY - heightDelta;
        alertWindow.moveTo(alertWindow.screenX, adjustedY);
      }
    }
  }

  let adjustedY = gOrigin & NS_ALERT_TOP ? aReplacedAlert.screenY :
                  aReplacedAlert.screenY - heightDelta;
  window.moveTo(aReplacedAlert.screenX, adjustedY);
}

function moveWindowToEnd() {
  // Determine position
  let x = gOrigin & NS_ALERT_LEFT ? screen.availLeft :
          screen.availLeft + screen.availWidth - window.outerWidth;
  let y = gOrigin & NS_ALERT_TOP ? screen.availTop :
          screen.availTop + screen.availHeight - window.outerHeight;

  // Position the window at the end of all alerts.
  let windows = Services.wm.getEnumerator("alert:alert");
  while (windows.hasMoreElements()) {
    let alertWindow = windows.getNext();
    if (alertWindow != window) {
      if (gOrigin & NS_ALERT_TOP) {
        y = Math.max(y, alertWindow.screenY + alertWindow.outerHeight - WINDOW_SHADOW_SPREAD);
      } else {
        y = Math.min(y, alertWindow.screenY - window.outerHeight + WINDOW_SHADOW_SPREAD);
      }
    }
  }

  // Offset the alert by WINDOW_MARGIN pixels from the edge of the screen
  y += gOrigin & NS_ALERT_TOP ? WINDOW_MARGIN : -WINDOW_MARGIN;
  x += gOrigin & NS_ALERT_LEFT ? WINDOW_MARGIN : -WINDOW_MARGIN;

  window.moveTo(x, y);
}

function onAlertBeforeUnload() {
  if (!gIsReplaced) {
    // Move other alert windows to fill the gap left by closing alert.
    let heightDelta = window.outerHeight + WINDOW_MARGIN - WINDOW_SHADOW_SPREAD;
    let windows = Services.wm.getEnumerator("alert:alert");
    while (windows.hasMoreElements()) {
      let alertWindow = windows.getNext();
      if (alertWindow != window) {
        if (gOrigin & NS_ALERT_TOP) {
          if (alertWindow.screenY > window.screenY) {
            alertWindow.moveTo(alertWindow.screenX, alertWindow.screenY - heightDelta);
          }
        } else if (window.screenY > alertWindow.screenY) {
          alertWindow.moveTo(alertWindow.screenX, alertWindow.screenY + heightDelta);
        }
      }
    }
  }

  if (gAlertListener) {
    gAlertListener.observe(null, "alertfinished", gAlertCookie);
  }
}

function onAlertClick() {
  if (gAlertListener && gAlertTextClickable) {
    gAlertListener.observe(null, "alertclickcallback", gAlertCookie);
  }

  let alertBox = document.getElementById("alertBox");
  if (alertBox.getAttribute("animate") == "true") {
    // Closed when the animation ends.
    alertBox.setAttribute("clicked", "true");
  } else {
    window.close();
  }
}

function doNotDisturb() {
  const alertService = Cc["@mozilla.org/alerts-service;1"]
                         .getService(Ci.nsIAlertsService)
                         .QueryInterface(Ci.nsIAlertsDoNotDisturb);
  alertService.manualDoNotDisturb = true;
  Services.telemetry.getHistogramById("WEB_NOTIFICATION_MENU")
                    .add(0);
  onAlertClose();
}

function disableForOrigin() {
  gAlertListener.observe(null, "alertdisablecallback", gAlertCookie);
  onAlertClose();
}

function onAlertSettingsFocus(event) {
  event.target.removeAttribute("focusedViaMouse");
}

function onAlertSettingsClick(event) {
  // XXXjaws Hack used to remove the focus-ring only
  // from mouse interaction, but focus-ring drawing
  // should only be enabled when interacting via keyboard.
  event.target.setAttribute("focusedViaMouse", true);
  event.stopPropagation();
}

function openSettings() {
  gAlertListener.observe(null, "alertsettingscallback", gAlertCookie);
  onAlertClose();
}

function onAlertClose() {
  let alertBox = document.getElementById("alertBox");
  if (alertBox.getAttribute("animate") == "true") {
    // Closed when the animation ends.
    alertBox.setAttribute("closing", "true");
  } else {
    window.close();
  }
}
