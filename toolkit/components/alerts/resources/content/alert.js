# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Scott MacGregor <mscott@netscape.com>
#   Jens Bannmann <jens.b@web.de>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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
  var prefService = Components.classes["@mozilla.org/preferences-service;1"].getService();
  prefService = prefService.QueryInterface(Components.interfaces.nsIPrefService);
  var prefBranch = prefService.getBranch(null);
  gSlideIncrement = prefBranch.getIntPref("alerts.slideIncrement");
  gSlideTime = prefBranch.getIntPref("alerts.slideIncrementTime");
  gOpenTime = prefBranch.getIntPref("alerts.totalOpenTime");
  gDisableSlideEffect = prefBranch.getBoolPref("alerts.disableSlidingEffect");
 
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
