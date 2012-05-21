# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

Components.utils.import("resource://gre/modules/Services.jsm");

// Copied from nsILookAndFeel.h, see comments on eMetric_AlertNotificationOrigin
const NS_ALERT_HORIZONTAL = 1;
const NS_ALERT_LEFT = 2;
const NS_ALERT_TOP = 4;

var gFinalSize;
var gCurrentSize = 1;

var gSlideIncrement = 1;
var gSlideTime = 10;
var gOpenTime = 3000; // total time the alert should stay up once we are done animating.
var gOrigin = 0; // Default value: alert from bottom right, sliding in vertically.
var gDisableSlideEffect = false;
 
var gAlertListener = null;
var gAlertTextClickable = false;
var gAlertCookie = "";

function prefillAlertInfo()
{
  // unwrap all the args....
  // arguments[0] --> the image src url
  // arguments[1] --> the alert title
  // arguments[2] --> the alert text
  // arguments[3] --> is the text clickable? 
  // arguments[4] --> the alert cookie to be passed back to the listener
  // arguments[5] --> the alert origin reported by the look and feel
  // arguments[6] --> an optional callback listener (nsIObserver)

  switch (window.arguments.length)
  {
    default:
    case 7:
      gAlertListener = window.arguments[6];
    case 6:
      gOrigin = window.arguments[5];
    case 5:
      gAlertCookie = window.arguments[4];
    case 4:
      gAlertTextClickable = window.arguments[3];
      if (gAlertTextClickable) {
        document.getElementById('alertNotification').setAttribute('clickable', true);
        document.getElementById('alertTextLabel').setAttribute('clickable', true);
      }
    case 3:
      document.getElementById('alertTextLabel').setAttribute('value', window.arguments[2]);
    case 2:
      document.getElementById('alertTitleLabel').setAttribute('value', window.arguments[1]);
    case 1:
      document.getElementById('alertImage').setAttribute('src', window.arguments[0]);
    case 0:
      break;
  }
}

function onAlertLoad()
{
  gSlideIncrement     = Services.prefs.getIntPref("alerts.slideIncrement");
  gSlideTime          = Services.prefs.getIntPref("alerts.slideIncrementTime");
  gOpenTime           = Services.prefs.getIntPref("alerts.totalOpenTime");
  gDisableSlideEffect = Services.prefs.getBoolPref("alerts.disableSlidingEffect");

  // Make sure that the contents are fixed at the window edge facing the
  // screen's center so that the window looks like "sliding in" and not
  // like "unfolding". The default packing of "start" only works for
  // vertical-bottom and horizontal-right positions, so we change it here.
  if (gOrigin & NS_ALERT_HORIZONTAL)
  {
    if (gOrigin & NS_ALERT_LEFT)
      document.documentElement.pack = "end";

    // Additionally, change the orientation so the packing works as intended
    document.documentElement.orient = "horizontal";
  }
  else
  {
    if (gOrigin & NS_ALERT_TOP)
      document.documentElement.pack = "end";
  }

  var alertBox = document.getElementById("alertBox");
  alertBox.orient = (gOrigin & NS_ALERT_HORIZONTAL) ? "vertical" : "horizontal";

  sizeToContent();

  // Work around a bug where sizeToContent() leaves a border outside of the content
  var contentDim = document.getElementById("alertBox").boxObject;
  if (window.innerWidth == contentDim.width + 1)
    --window.innerWidth;

  // Start with a 1px width/height, because 0 causes trouble with gtk1/2
  gCurrentSize = 1;

  // Determine final size
  if (gOrigin & NS_ALERT_HORIZONTAL)
  {
    gFinalSize = window.outerWidth;
    window.outerWidth = gCurrentSize;
  }
  else
  {
    gFinalSize = window.outerHeight;
    window.outerHeight = gCurrentSize;
  }

  // Determine position
  var x = gOrigin & NS_ALERT_LEFT ? screen.availLeft :
          screen.availLeft + screen.availWidth - window.outerWidth;
  var y = gOrigin & NS_ALERT_TOP ? screen.availTop :
          screen.availTop + screen.availHeight - window.outerHeight;

  // Offset the alert by 10 pixels from the edge of the screen
  if (gOrigin & NS_ALERT_HORIZONTAL)
    y += gOrigin & NS_ALERT_TOP ? 10 : -10;
  else
    x += gOrigin & NS_ALERT_LEFT ? 10 : -10;

  window.moveTo(x, y);

  setTimeout(animateAlert, gSlideTime);
}

function animate(step)
{
  gCurrentSize += step;

  if (gFinalSize < gCurrentSize)
    gCurrentSize = gFinalSize;

  if (gOrigin & NS_ALERT_HORIZONTAL)
  {
    if (!(gOrigin & NS_ALERT_LEFT))
      window.screenX -= step;
    window.outerWidth = gCurrentSize;
  }
  else
  {
    if (!(gOrigin & NS_ALERT_TOP))
      window.screenY -= step;
    window.outerHeight = gCurrentSize;
  }
}

function animateAlert()
{
  if (gCurrentSize < gFinalSize)
  {
    if (gDisableSlideEffect)
      animate(gFinalSize); // We don't begin on zero.
    else
      animate(gSlideIncrement);
    setTimeout(animateAlert, gSlideTime);
  }
  else
    setTimeout(animateCloseAlert, gOpenTime);  
}

function animateCloseAlert()
{
  if (gCurrentSize > 1 && !gDisableSlideEffect)
  {
    animate(-gSlideIncrement);
    setTimeout(animateCloseAlert, gSlideTime);
  }
  else
    closeAlert();
}

function closeAlert() {
  if (gAlertListener)
    gAlertListener.observe(null, "alertfinished", gAlertCookie); 
  window.close(); 
}

function onAlertClick()
{
  if (gAlertListener && gAlertTextClickable)
    gAlertListener.observe(null, "alertclickcallback", gAlertCookie);
  closeAlert();
}
